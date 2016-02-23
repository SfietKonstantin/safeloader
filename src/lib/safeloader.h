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

#ifndef SAFELOADER_H
#define SAFELOADER_H

#include <QQmlEngine>
#include <QQuickFramebufferObject>
#include <QQuickRenderControl>
#include <QQuickWindow>
#include "qobjectptr.h"

class QSurface;
class SafeLoader: public QQuickFramebufferObject
{
    Q_OBJECT
    Q_PROPERTY(QString source READ source WRITE setSource NOTIFY sourceChanged)
public:
    explicit SafeLoader(QQuickItem *parent = nullptr);
    Renderer * createRenderer() const;
    QString source() const;
    void setSource(const QString &source);
signals:
    void sourceChanged();
private:
    class Renderer : public QQuickFramebufferObject::Renderer
    {
    public:
        Renderer();
        ~Renderer();
        void render() override;
        QOpenGLFramebufferObject * createFramebufferObject(const QSize &size) override;
        void synchronize(QQuickFramebufferObject *object) override;
    private:
        void cleanup();

        QObjectPtr<QQuickRenderControl> m_renderControl {};
        QObjectPtr<QQuickWindow> m_window {};
        QQuickWindow *m_surface {nullptr};

        QMetaObject::Connection m_sceneChangedConnection {};
        QMetaObject::Connection m_renderRequestedConnection {};
    };
    void createComponent();
    void run();
    QObjectPtr<QQmlEngine> m_engine {};
    QObjectPtr<QQmlComponent> m_component {};
    QObjectPtr<QQuickItem> m_rootItem {};
    QString m_source;
};

#endif // SAFELOADER_H
