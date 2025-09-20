/*
 * SPDX-FileCopyrightText: 2023 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
 */

#include "mpvrenderer.h"
#include "mpvabstractitem.h"
#include "mpvabstractitem_p.h"
#include "mpvcontroller.h"

#include <QGuiApplication>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QQuickWindow>
#include <QThread>

static void *get_proc_address_mpv(void *ctx, const char *name)
{
    MpvRenderer *renderer = static_cast<MpvRenderer *>(ctx);
    if (!renderer) {
        return nullptr;
    }

    QOpenGLContext *glctx = renderer->context();
    if (!glctx) {
        return nullptr;
    }

    // Make sure the context is current on this thread before calling getProcAddress.
    if (glctx->thread() == QThread::currentThread()) {
        MpvAbstractItem *item = renderer->mpvAbstractItem();
        if (item && item->window()) {
            glctx->makeCurrent(item->window());
        }
    }

    return reinterpret_cast<void *>(glctx->getProcAddress(QByteArray(name)));
}

void on_mpv_redraw(void *ctx)
{
    QMetaObject::invokeMethod(static_cast<MpvAbstractItem *>(ctx), &MpvAbstractItem::update, Qt::QueuedConnection);
}

MpvRenderer::MpvRenderer(MpvAbstractItem *new_obj)
    : m_mpvAItem{new_obj}
{
}

void MpvRenderer::synchronize(QQuickFramebufferObject *item)
{
    Q_UNUSED(item);
}

void MpvRenderer::render()
{
    static bool thread_id_printed = false;
    if (!thread_id_printed) {
        fprintf(stderr, "\n[PROOF] The REAL Qt Render Thread ID is: %p\n\n", (void*)QThread::currentThreadId());
        fflush(stderr);
        thread_id_printed = true;
    }
    // --- PROOF END ---    
    // LAZY INITIALIZATION: This only runs once.
    if (!m_mpvAItem->d_ptr->m_mpv_gl) {
        // Initialize the OpenGL callback for the advanced control
        if (!m_context) {
            m_context = QOpenGLContext::currentContext();
            if (!m_context) {
                qFatal("Could not get QOpenGLContext on the render thread!");
            }
        }
        mpv_opengl_init_params gl_init_params;
        gl_init_params.get_proc_address = get_proc_address_mpv;
        gl_init_params.get_proc_address_ctx = this;
#if MPV_CLIENT_API_VERSION < MPV_MAKE_VERSION(2, 0)
        gl_init_params.extra_exts = nullptr;
#endif
        char backend_name[] = "gpu-next";
        mpv_render_param display{MPV_RENDER_PARAM_INVALID, nullptr};
#if defined(Q_OS_UNIX) && !defined(Q_OS_DARWIN) && !defined(Q_OS_ANDROID) && !defined(Q_OS_HAIKU)
        if (QGuiApplication::platformName() == QStringLiteral("xcb")) {
            display.type = MPV_RENDER_PARAM_X11_DISPLAY;
            display.data = qGuiApp->nativeInterface<QNativeInterface::QX11Application>()->display();
        }
#endif
        static const int advanced_control_flag = 1;
        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL)},
            {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_init_params},
            {MPV_RENDER_PARAM_ADVANCED_CONTROL, (void*)&advanced_control_flag},
            {MPV_RENDER_PARAM_BACKEND, backend_name},
            display,
            {MPV_RENDER_PARAM_INVALID, nullptr}
        };
        if (mpv_render_context_create(&m_mpvAItem->d_ptr->m_mpv_gl, m_mpvAItem->d_ptr->m_mpv, params) < 0) {
            qFatal("failed to initialize mpv GL context");
        }
        mpv_render_context_set_update_callback(m_mpvAItem->d_ptr->m_mpv_gl, on_mpv_redraw, m_mpvAItem);
        Q_EMIT m_mpvAItem->ready();
    }

    mpv_render_context_update(m_mpvAItem->d_ptr->m_mpv_gl);

    QOpenGLFramebufferObject *fbo = framebufferObject();
    mpv_opengl_fbo mpfbo;
    mpfbo.fbo = static_cast<int>(fbo->handle());
    mpfbo.w = fbo->width();
    mpfbo.h = fbo->height();
    mpfbo.internal_format = 0;

    int flip_y{0};

    mpv_render_param render_params[] = {
        {MPV_RENDER_PARAM_OPENGL_FBO, &mpfbo},
        {MPV_RENDER_PARAM_FLIP_Y, &flip_y},
        {MPV_RENDER_PARAM_INVALID, nullptr}
    };

    // Render the current mpv frame to our FBO.
    mpv_render_context_render(m_mpvAItem->d_ptr->m_mpv_gl, render_params);

    // Signal to mpv that we have finished rendering and presented the frame.
    // This completes the handshake and unstalls the internal state machine, allowing
    // the dispatch queue to function correctly for the *next* frame.
    mpv_render_context_report_swap(m_mpvAItem->d_ptr->m_mpv_gl);

    // This is the Qt update call, which is separate from mpv's.
    update();
}

QOpenGLFramebufferObject *MpvRenderer::createFramebufferObject(const QSize &size)
{
    QOpenGLFramebufferObjectFormat format;
    format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
    return new QOpenGLFramebufferObject(size, format);
}