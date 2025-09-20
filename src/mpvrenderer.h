/*
 * SPDX-FileCopyrightText: 2023 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
 */

#ifndef MPVRENDERER_H
#define MPVRENDERER_H

#include <QtQuick/QQuickFramebufferObject>
#include <QByteArray>
#include <QOpenGLContext> // Include for QOpenGLContext

class MpvAbstractItem;

class MpvRenderer : public QQuickFramebufferObject::Renderer
{
public:
    explicit MpvRenderer(MpvAbstractItem *new_obj);
    ~MpvRenderer() = default;

    void render() override;
    void synchronize(QQuickFramebufferObject *item) override;

    // This function is called when a new FBO is needed.
    // This happens on the initial frame.
    QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) override;

    // Public getters for internal members (used by static callbacks, etc.)
    MpvAbstractItem *mpvAbstractItem() const { return m_mpvAItem; }
    QOpenGLContext *context() const { return m_context; }

private:
    MpvAbstractItem *m_mpvAItem{nullptr};
    QOpenGLContext *m_context{nullptr};

    // Store the backend name as a QByteArray to ensure its memory remains valid
    QByteArray m_backendNameUtf8;
    const char *m_backendNamePtr = nullptr;
};

#endif // MPVRENDERER_H
