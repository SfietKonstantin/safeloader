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

#include <QOpenGLFramebufferObject>
#include <QQuickWindow>
#include <QOpenGLContext>
#include <QLoggingCategory>
#include <QCloseEvent>

static const QLoggingCategory logger {"safeloader"};

class TestRenderControl : public QQuickRenderControl
{
public:
    TestRenderControl(QWindow &window)
        : m_window(window)
    {
    }
    QWindow * renderWindow(QPoint *offset) override
    {
        if (offset) {
            *offset = QPoint(0, 0);
        }
        return &m_window;
    }
private:
    QWindow &m_window;
};

SafeLoader::Renderer::Renderer()
{
}

SafeLoader::Renderer::~Renderer()
{
    disconnect(m_sceneChangedConnection);
    disconnect(m_renderRequestedConnection);
    m_renderControl.reset(); // TODO: Dangerous in a multithreaded context. We need to call invalidate before
    m_window.reset();
}

void SafeLoader::Renderer::render()
{
    QOpenGLFramebufferObject *fbo = framebufferObject();
    if (m_window->renderTarget() != fbo) {
        m_window->setRenderTarget(fbo);
    }

    // TODO: this is dangerous we need to do it in the main thread
    m_renderControl->polishItems();
    m_renderControl->sync();
    m_renderControl->render();
}

QOpenGLFramebufferObject * SafeLoader::Renderer::createFramebufferObject(const QSize &size)
{
    QOpenGLFramebufferObjectFormat format;
    format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
    format.setSamples(4);
    return new QOpenGLFramebufferObject(size, format);
}

void SafeLoader::Renderer::synchronize(QQuickFramebufferObject *object)
{
    SafeLoader *safeLoader = static_cast<SafeLoader *>(object);

    if (m_surface == nullptr) {
        Q_ASSERT(safeLoader->window());
        m_surface = safeLoader->window();
    }

    if (m_surface != safeLoader->window()) {
        qCWarning(logger) << "Surface is not expected to change";
    }

    if (!m_renderControl && !m_window) {
        m_renderControl.reset(new TestRenderControl(*safeLoader->window()));

        m_window.reset(new QQuickWindow(m_renderControl.get()));
        if (safeLoader->m_engine->incubationController() == nullptr) {
            safeLoader->m_engine->setIncubationController(m_window->incubationController());
        }

        m_renderControl->initialize(QOpenGLContext::currentContext());
        m_sceneChangedConnection = QObject::connect(m_renderControl.get(), &QQuickRenderControl::sceneChanged, [this]() { update(); });
        m_renderRequestedConnection = QObject::connect(m_renderControl.get(), &QQuickRenderControl::renderRequested, [this]() { update(); });
    }

    QRect geometry = QRect(0, 0, safeLoader->width(), safeLoader->height());
    m_window->setGeometry(geometry);
    m_window->contentItem()->setWidth(geometry.width());
    m_window->contentItem()->setHeight(geometry.height());

    if (safeLoader->m_rootItem) {
        if (safeLoader->m_rootItem->parentItem() == nullptr) {
            safeLoader->m_rootItem->setParentItem(m_window->contentItem());
        }
        safeLoader->m_rootItem->setWidth(geometry.width());
        safeLoader->m_rootItem->setHeight(geometry.height());
    }
}

void SafeLoader::Renderer::cleanup()
{
    // TODO: perform the cleanup
    /*
    Q_ASSERT(m_context->makeCurrent(m_surface));
    m_renderControl->invalidate();
    */
}

SafeLoader::SafeLoader(QQuickItem *parent)
    : QQuickFramebufferObject(parent)
    , m_engine(new QQmlEngine())
{
}

QQuickFramebufferObject::Renderer * SafeLoader::createRenderer() const
{
    return new Renderer();
}

QString SafeLoader::source() const
{
    return m_source;
}

void SafeLoader::setSource(const QString &source)
{
    if (m_source != source) {
        m_source = source;
        emit sourceChanged();
        createComponent();
    }
}

void SafeLoader::createComponent()
{
    m_component.reset(new QQmlComponent(m_engine.get(), QUrl(m_source)));
    if (m_component->isLoading()) {
        connect(m_component.get(), &QQmlComponent::statusChanged, [this](QQmlComponent::Status) { run(); });
    } else {
        run();
    }
}

void SafeLoader::run()
{
    if (m_component->isError()) {
        QList<QQmlError> errorList = m_component->errors();
        foreach (const QQmlError &error, errorList) {
            qCWarning(logger) << error.url() << error.line() << error;
        }
        return;
    }

    QObject *object = m_component->create();
    if (m_component->isError()) {
        QList<QQmlError> errorList = m_component->errors();
        foreach (const QQmlError &error, errorList) {
            qCWarning(logger) << error.url() << error.line() << error;
        }
        return;
    }

    QQuickItem *item = qobject_cast<QQuickItem *>(object);
    if (!item) {
        qCWarning(logger) << "Not a QQuickItem";
        delete object;
        return;
    }
    m_rootItem.reset(item);
    update();
}
