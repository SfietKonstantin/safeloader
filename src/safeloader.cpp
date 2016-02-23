/*
 * Copyright (C) 2015 Lucien XU <sfietkonstantin@free.fr>
 *
 * You may use this file under the terms of the BSD license as follows:
 *
 * "Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * The names of its contributors may not be used to endorse or promote
 *     products derived from this software without specific prior written
 *     permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
 */

#include "safeloader.h"
#include <QQmlEngine>
#include <QQuickRenderControl>
#include <QQuickWindow>
#include <QOffscreenSurface>
#include <QLoggingCategory>
#include <QSGSimpleTextureNode>
#include <QQuickFramebufferObject>
#include <QCoreApplication>
#include <private/qquickitem_p.h>
#include "qobjectptr.h"

Q_LOGGING_CATEGORY(log, "safeloader")


class SafeLoaderNode : public QObject, public QSGSimpleTextureNode
{
    Q_OBJECT
public:
    SafeLoaderNode(QObject *parent = nullptr)
        : QObject(parent)
    {
        qsgnode_set_description(this, QStringLiteral("safeloadernode"));
    }
    ~SafeLoaderNode()
    {
        delete texture();
    }
    void scheduleRender()
    {
        renderPending = true;
        window->update();
    }
public slots:
    void render()
    {
        qCDebug(log) << "Render";

        if (renderPending) {
            qCDebug(log) << "Perform rendering";

            renderPending = false;
            fbo->bind();
            QOpenGLContext::currentContext()->functions()->glViewport(0, 0, fbo->width(), fbo->height());
            // renderer->render();
            fbo->bindDefault();
            if (displayFbo != nullptr) {
                QOpenGLFramebufferObject::blitFramebuffer(displayFbo.get(), fbo.get());
            }
            markDirty(QSGNode::DirtyMaterial);
        }
    }
    void handleScreenChange()
    {
        if (window->effectiveDevicePixelRatio() != devicePixelRatio) {
            // renderer->invalidateFramebufferObject();
            safeLoader->update();
        }
    }
public:
    QQuickWindow *window {nullptr};
    std::unique_ptr<QOpenGLFramebufferObject> fbo {};
    std::unique_ptr<QOpenGLFramebufferObject> displayFbo {};
    SafeLoader *safeLoader {nullptr};
    bool renderPending {true};
    qreal devicePixelRatio {1.};
};

class SafeLoaderRenderControl : public QQuickRenderControl
{
public:
    explicit SafeLoaderRenderControl(QWindow *w)
        : m_window(w)
    {
    }
    QWindow * renderWindow(QPoint *offset) override
    {
        if (offset) {
            *offset = QPoint(0, 0);
        }
        return m_window;
    }
private:
    QWindow *m_window;
};

class SafeLoaderPrivate: public QQuickItemPrivate
{
public:
    explicit SafeLoaderPrivate(SafeLoader *q)
        : QQuickItemPrivate(), q_ptr(q)
    {
    }
    static std::unique_ptr<QOpenGLFramebufferObject> createFramebuffer(const QSize &size)
    {
        QOpenGLFramebufferObjectFormat format;
        format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
        format.setSamples(4);
        return std::unique_ptr<QOpenGLFramebufferObject>(new QOpenGLFramebufferObject(size, format));
    }
    void createFbo()
    {
        qCDebug(log) << "Offscreen create FBO";
    }
    void destroyFbo()
    {
        qCDebug(log) << "Offscreen destroy FBO";
    }
    void requestUpdate()
    {
        qCDebug(log) << "Offscreen request update";
    }
    void startQuick(const QString &filename)
    {
        m_qmlComponent = new QQmlComponent(m_qmlEngine, QUrl(filename));
        if (m_qmlComponent->isLoading())
            QObject::connect(m_qmlComponent, &QQmlComponent::statusChanged, [this](QQmlComponent::Status) { run(); });
        else
            run();
    }
    void run()
    {
        if (m_qmlComponent->isError()) {
            QList<QQmlError> errorList = m_qmlComponent->errors();
            foreach (const QQmlError &error, errorList) {
                qCWarning(log) << error.url() << error.line() << error;
            }
            return;
        }

        QObject *rootObject = m_qmlComponent->create();
        if (m_qmlComponent->isError()) {
            QList<QQmlError> errorList = m_qmlComponent->errors();
            foreach (const QQmlError &error, errorList) {
                qCWarning(log) << error.url() << error.line() << error;
            }
            return;
        }

        m_rootItem = qobject_cast<QQuickItem *>(rootObject);
        if (!m_rootItem) {
            qCWarning(log) << "run: Not a QQuickItem";
            delete rootObject;
            return;
        }

        // The root item is ready. Associate it with the window.
        m_rootItem->setParentItem(m_quickWindow->contentItem());

        // Update item and rendering related geometries.
        updateSizes();

        // Initialize the render control and our OpenGL resources.
        m_context->makeCurrent(m_offscreenSurface);
        m_renderControl->initialize(m_context);
    }
    void updateSizes()
    {
        Q_Q(SafeLoader);
        qCDebug(log) << "Updating size" << q->width() << q->height();
        // Behave like SizeRootObjectToView.
        m_rootItem->setWidth(q->width());
        m_rootItem->setHeight(q->height());

        m_quickWindow->setGeometry(0, 0, q->width(), q->height());
    }
    QUrl source {};
    SafeLoaderNode *node {nullptr};

    QOpenGLContext *m_context {nullptr};
    QOffscreenSurface *m_offscreenSurface {nullptr};
    QQuickRenderControl *m_renderControl {nullptr};
    QQuickWindow *m_quickWindow {nullptr};
    QQmlEngine *m_qmlEngine {nullptr};
    QQmlComponent *m_qmlComponent {nullptr};
    QQuickItem *m_rootItem {nullptr};
private:
    SafeLoader *q_ptr {nullptr};
    Q_DECLARE_PUBLIC(SafeLoader)
};

SafeLoader::SafeLoader(QQuickItem *parent)
    : QQuickItem(*(new SafeLoaderPrivate(this)), parent)
{
    Q_D(SafeLoader);
    setFlag(ItemHasContents);

    QSurfaceFormat format;
    // Qt Quick may need a depth and stencil buffer. Always make sure these are available.
    format.setDepthBufferSize(16);
    format.setStencilBufferSize(8);

    d->m_context = new QOpenGLContext;
    d->m_context->setFormat(format);
    d->m_context->create();

    d->m_offscreenSurface = new QOffscreenSurface;
    // Pass m_context->format(), not format. Format does not specify and color buffer
    // sizes, while the context, that has just been created, reports a format that has
    // these values filled in. Pass this to the offscreen surface to make sure it will be
    // compatible with the context's configuration.
    d->m_offscreenSurface->setFormat(d->m_context->format());
    d->m_offscreenSurface->create();

    d->m_renderControl = new SafeLoaderRenderControl(window());

    // Create a QQuickWindow that is associated with out render control. Note that this
    // window never gets created or shown, meaning that it will never get an underlying
    // native (platform) window.
    d->m_quickWindow = new QQuickWindow(d->m_renderControl);

    // Create a QML engine.
    d->m_qmlEngine = new QQmlEngine;
    if (!d->m_qmlEngine->incubationController()) {
        d->m_qmlEngine->setIncubationController(d->m_quickWindow->incubationController());
    }

    connect(this, &QQuickItem::widthChanged, [d]() { d->updateSizes(); });
    connect(this, &QQuickItem::heightChanged, [d]() { d->updateSizes(); });

    // Now hook up the signals. For simplicy we don't differentiate between
    // renderRequested (only render is needed, no sync) and sceneChanged (polish and sync
    // is needed too).
    connect(d->m_quickWindow, &QQuickWindow::sceneGraphInitialized, [d]() { d->createFbo(); });
    connect(d->m_quickWindow, &QQuickWindow::sceneGraphInvalidated, [d]() { d->destroyFbo(); });
    connect(d->m_renderControl, &QQuickRenderControl::renderRequested, [d]() { d->requestUpdate(); });
    connect(d->m_renderControl, &QQuickRenderControl::sceneChanged, [d]() { d->requestUpdate(); });

    d->startQuick(QLatin1String("qrc:/test.qml"));
}

SafeLoader::~SafeLoader()
{
    Q_D(SafeLoader);
    d->m_context->makeCurrent(d->m_offscreenSurface);
    delete d->m_renderControl;

    delete d->m_qmlComponent;
    delete d->m_quickWindow;
    delete d->m_qmlEngine;

    d->m_context->doneCurrent();
    delete d->m_offscreenSurface;
    delete d->m_context;

}

QUrl SafeLoader::source() const
{
    Q_D(const SafeLoader);
    return d->source;
}

bool SafeLoader::event(QEvent *e)
{
    Q_D(SafeLoader);
    if (e->type() == QEvent::User) {
        d->updateSizes();
        return true;
    }
    return QQuickItem::event(e);
}

void SafeLoader::setSource(const QUrl &source)
{
    Q_D(SafeLoader);
    if (d->source != source) {
        d->source = source;
        emit sourceChanged();
    }
}

QSGNode * SafeLoader::updatePaintNode(QSGNode *node, QQuickItem::UpdatePaintNodeData *data)
{
    Q_D(SafeLoader);
    Q_UNUSED(data)
    SafeLoaderNode *safeLoaderNode = static_cast<SafeLoaderNode *>(node);
    if (safeLoaderNode == nullptr) {
        qCDebug(log) << "Creating node";
        Q_ASSERT(d->node == nullptr);
        d->node = new SafeLoaderNode();
        safeLoaderNode = d->node;
        safeLoaderNode->window = window();
        safeLoaderNode->safeLoader = this;
        connect(window(), SIGNAL(beforeRendering()), safeLoaderNode, SLOT(render()));
        connect(window(), SIGNAL(screenChanged(QScreen*)), safeLoaderNode, SLOT(handleScreenChange()));
    }

    QSize minFboSize = d->sceneGraphContext()->minimumFBOSize();
    QSize fboSize(qMax<int>(minFboSize.width(), width()),
                         qMax<int>(minFboSize.height(), height()));
    safeLoaderNode->devicePixelRatio = window()->effectiveDevicePixelRatio();
    fboSize *= safeLoaderNode->devicePixelRatio;
    qCDebug(log) << "FBO size:" << fboSize;
    if (safeLoaderNode->fbo) {
        if (safeLoaderNode->fbo->size() != fboSize) {
            qCDebug(log) << "FBO size changed, resetting FBOs";
            safeLoaderNode->fbo.reset();
            safeLoaderNode->displayFbo.reset();
        }
    }
    if (!safeLoaderNode->fbo) {
        qCDebug(log) << "No FBO, creating FBOs";
        safeLoaderNode->fbo = std::move(d->createFramebuffer(fboSize));
        GLuint displayTexture = safeLoaderNode->fbo->texture();
        if (safeLoaderNode->fbo->format().samples() > 0) {
            safeLoaderNode->displayFbo.reset(new QOpenGLFramebufferObject(safeLoaderNode->fbo->size()));
            displayTexture = safeLoaderNode->displayFbo->texture();
        }
        safeLoaderNode->setTexture(window()->createTextureFromId(displayTexture,
                                                                 safeLoaderNode->fbo->size(),
                                                                 QQuickWindow::TextureHasAlphaChannel));
    }
    safeLoaderNode->setTextureCoordinatesTransform(QSGSimpleTextureNode::NoTransform);
    safeLoaderNode->setFiltering(d->smooth ? QSGTexture::Linear : QSGTexture::Nearest);
    safeLoaderNode->setRect(0, 0, width(), height());
    safeLoaderNode->scheduleRender();

    return safeLoaderNode;
}

#include "safeloader.moc"
