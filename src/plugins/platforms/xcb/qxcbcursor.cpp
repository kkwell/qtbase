/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qxcbcursor.h"
#include "qxcbconnection.h"
#include "qxcbwindow.h"
#include "qxcbimage.h"
#include "qxcbxsettings.h"

#if QT_CONFIG(library)
#include <QtCore/QLibrary>
#endif
#include <QtGui/QWindow>
#include <QtGui/QBitmap>
#include <QtGui/private/qguiapplication_p.h>
#include <X11/cursorfont.h>
#include <xcb/xfixes.h>
#include <xcb/xcb_image.h>

QT_BEGIN_NAMESPACE

typedef int (*PtrXcursorLibraryLoadCursor)(void *, const char *);
typedef char *(*PtrXcursorLibraryGetTheme)(void *);
typedef int (*PtrXcursorLibrarySetTheme)(void *, const char *);
typedef int (*PtrXcursorLibraryGetDefaultSize)(void *);

#if defined(XCB_USE_XLIB) && QT_CONFIG(library)
#include <X11/Xlib.h>
enum {
    XCursorShape = CursorShape
};
#undef CursorShape

static PtrXcursorLibraryLoadCursor ptrXcursorLibraryLoadCursor = 0;
static PtrXcursorLibraryGetTheme ptrXcursorLibraryGetTheme = 0;
static PtrXcursorLibrarySetTheme ptrXcursorLibrarySetTheme = 0;
static PtrXcursorLibraryGetDefaultSize ptrXcursorLibraryGetDefaultSize = 0;
#endif

static xcb_font_t cursorFont = 0;
static int cursorCount = 0;

#ifndef QT_NO_CURSOR

static uint8_t cur_blank_bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

static const uint8_t cur_ver_bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x80, 0x01, 0xc0, 0x03, 0xe0, 0x07, 0xf0, 0x0f,
    0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0xf0, 0x0f,
    0xe0, 0x07, 0xc0, 0x03, 0x80, 0x01, 0x00, 0x00 };
static const uint8_t mcur_ver_bits[] = {
    0x00, 0x00, 0x80, 0x03, 0xc0, 0x07, 0xe0, 0x0f, 0xf0, 0x1f, 0xf8, 0x3f,
    0xfc, 0x7f, 0xc0, 0x07, 0xc0, 0x07, 0xc0, 0x07, 0xfc, 0x7f, 0xf8, 0x3f,
    0xf0, 0x1f, 0xe0, 0x0f, 0xc0, 0x07, 0x80, 0x03 };
static const uint8_t cur_hor_bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x08, 0x30, 0x18,
    0x38, 0x38, 0xfc, 0x7f, 0xfc, 0x7f, 0x38, 0x38, 0x30, 0x18, 0x20, 0x08,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
static const uint8_t mcur_hor_bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x40, 0x04, 0x60, 0x0c, 0x70, 0x1c, 0x78, 0x3c,
    0xfc, 0x7f, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfc, 0x7f, 0x78, 0x3c,
    0x70, 0x1c, 0x60, 0x0c, 0x40, 0x04, 0x00, 0x00 };
static const uint8_t cur_bdiag_bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x00, 0x3e, 0x00, 0x3c, 0x00, 0x3e,
    0x00, 0x37, 0x88, 0x23, 0xd8, 0x01, 0xf8, 0x00, 0x78, 0x00, 0xf8, 0x00,
    0xf8, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
static const uint8_t mcur_bdiag_bits[] = {
    0x00, 0x00, 0xc0, 0x7f, 0x80, 0x7f, 0x00, 0x7f, 0x00, 0x7e, 0x04, 0x7f,
    0x8c, 0x7f, 0xdc, 0x77, 0xfc, 0x63, 0xfc, 0x41, 0xfc, 0x00, 0xfc, 0x01,
    0xfc, 0x03, 0xfc, 0x07, 0x00, 0x00, 0x00, 0x00 };
static const uint8_t cur_fdiag_bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x01, 0xf8, 0x00, 0x78, 0x00,
    0xf8, 0x00, 0xd8, 0x01, 0x88, 0x23, 0x00, 0x37, 0x00, 0x3e, 0x00, 0x3c,
    0x00, 0x3e, 0x00, 0x3f, 0x00, 0x00, 0x00, 0x00 };
static const uint8_t mcur_fdiag_bits[] = {
    0x00, 0x00, 0x00, 0x00, 0xfc, 0x07, 0xfc, 0x03, 0xfc, 0x01, 0xfc, 0x00,
    0xfc, 0x41, 0xfc, 0x63, 0xdc, 0x77, 0x8c, 0x7f, 0x04, 0x7f, 0x00, 0x7e,
    0x00, 0x7f, 0x80, 0x7f, 0xc0, 0x7f, 0x00, 0x00 };
static const uint8_t *cursor_bits16[] = {
    cur_ver_bits, mcur_ver_bits, cur_hor_bits, mcur_hor_bits,
    cur_bdiag_bits, mcur_bdiag_bits, cur_fdiag_bits, mcur_fdiag_bits,
    0, 0, cur_blank_bits, cur_blank_bits };

static const uint8_t vsplit_bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x80, 0x00, 0x00, 0x00, 0xc0, 0x01, 0x00, 0x00, 0xe0, 0x03, 0x00,
    0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00,
    0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0xff, 0x7f, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x7f, 0x00,
    0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00,
    0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0xe0, 0x03, 0x00,
    0x00, 0xc0, 0x01, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
static const uint8_t vsplitm_bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00,
    0x00, 0xc0, 0x01, 0x00, 0x00, 0xe0, 0x03, 0x00, 0x00, 0xf0, 0x07, 0x00,
    0x00, 0xf8, 0x0f, 0x00, 0x00, 0xc0, 0x01, 0x00, 0x00, 0xc0, 0x01, 0x00,
    0x00, 0xc0, 0x01, 0x00, 0x80, 0xff, 0xff, 0x00, 0x80, 0xff, 0xff, 0x00,
    0x80, 0xff, 0xff, 0x00, 0x80, 0xff, 0xff, 0x00, 0x80, 0xff, 0xff, 0x00,
    0x80, 0xff, 0xff, 0x00, 0x00, 0xc0, 0x01, 0x00, 0x00, 0xc0, 0x01, 0x00,
    0x00, 0xc0, 0x01, 0x00, 0x00, 0xf8, 0x0f, 0x00, 0x00, 0xf0, 0x07, 0x00,
    0x00, 0xe0, 0x03, 0x00, 0x00, 0xc0, 0x01, 0x00, 0x00, 0x80, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
static const uint8_t hsplit_bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x02, 0x00, 0x00, 0x40, 0x02, 0x00,
    0x00, 0x40, 0x02, 0x00, 0x00, 0x40, 0x02, 0x00, 0x00, 0x40, 0x02, 0x00,
    0x00, 0x41, 0x82, 0x00, 0x80, 0x41, 0x82, 0x01, 0xc0, 0x7f, 0xfe, 0x03,
    0x80, 0x41, 0x82, 0x01, 0x00, 0x41, 0x82, 0x00, 0x00, 0x40, 0x02, 0x00,
    0x00, 0x40, 0x02, 0x00, 0x00, 0x40, 0x02, 0x00, 0x00, 0x40, 0x02, 0x00,
    0x00, 0x40, 0x02, 0x00, 0x00, 0x40, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
static const uint8_t hsplitm_bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0xe0, 0x07, 0x00, 0x00, 0xe0, 0x07, 0x00, 0x00, 0xe0, 0x07, 0x00,
    0x00, 0xe0, 0x07, 0x00, 0x00, 0xe2, 0x47, 0x00, 0x00, 0xe3, 0xc7, 0x00,
    0x80, 0xe3, 0xc7, 0x01, 0xc0, 0xff, 0xff, 0x03, 0xe0, 0xff, 0xff, 0x07,
    0xc0, 0xff, 0xff, 0x03, 0x80, 0xe3, 0xc7, 0x01, 0x00, 0xe3, 0xc7, 0x00,
    0x00, 0xe2, 0x47, 0x00, 0x00, 0xe0, 0x07, 0x00, 0x00, 0xe0, 0x07, 0x00,
    0x00, 0xe0, 0x07, 0x00, 0x00, 0xe0, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
static const uint8_t whatsthis_bits[] = {
    0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x05, 0xf0, 0x07, 0x00,
    0x09, 0x18, 0x0e, 0x00, 0x11, 0x1c, 0x0e, 0x00, 0x21, 0x1c, 0x0e, 0x00,
    0x41, 0x1c, 0x0e, 0x00, 0x81, 0x1c, 0x0e, 0x00, 0x01, 0x01, 0x07, 0x00,
    0x01, 0x82, 0x03, 0x00, 0xc1, 0xc7, 0x01, 0x00, 0x49, 0xc0, 0x01, 0x00,
    0x95, 0xc0, 0x01, 0x00, 0x93, 0xc0, 0x01, 0x00, 0x21, 0x01, 0x00, 0x00,
    0x20, 0xc1, 0x01, 0x00, 0x40, 0xc2, 0x01, 0x00, 0x40, 0x02, 0x00, 0x00,
    0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, };
static const uint8_t whatsthism_bits[] = {
    0x01, 0x00, 0x00, 0x00, 0x03, 0xf0, 0x07, 0x00, 0x07, 0xf8, 0x0f, 0x00,
    0x0f, 0xfc, 0x1f, 0x00, 0x1f, 0x3e, 0x1f, 0x00, 0x3f, 0x3e, 0x1f, 0x00,
    0x7f, 0x3e, 0x1f, 0x00, 0xff, 0x3e, 0x1f, 0x00, 0xff, 0x9d, 0x0f, 0x00,
    0xff, 0xc3, 0x07, 0x00, 0xff, 0xe7, 0x03, 0x00, 0x7f, 0xe0, 0x03, 0x00,
    0xf7, 0xe0, 0x03, 0x00, 0xf3, 0xe0, 0x03, 0x00, 0xe1, 0xe1, 0x03, 0x00,
    0xe0, 0xe1, 0x03, 0x00, 0xc0, 0xe3, 0x03, 0x00, 0xc0, 0xe3, 0x03, 0x00,
    0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, };
static const uint8_t busy_bits[] = {
    0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00,
    0x09, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x21, 0x00, 0x00, 0x00,
    0x41, 0xe0, 0xff, 0x00, 0x81, 0x20, 0x80, 0x00, 0x01, 0xe1, 0xff, 0x00,
    0x01, 0x42, 0x40, 0x00, 0xc1, 0x47, 0x40, 0x00, 0x49, 0x40, 0x55, 0x00,
    0x95, 0x80, 0x2a, 0x00, 0x93, 0x00, 0x15, 0x00, 0x21, 0x01, 0x0a, 0x00,
    0x20, 0x01, 0x11, 0x00, 0x40, 0x82, 0x20, 0x00, 0x40, 0x42, 0x44, 0x00,
    0x80, 0x41, 0x4a, 0x00, 0x00, 0x40, 0x55, 0x00, 0x00, 0xe0, 0xff, 0x00,
    0x00, 0x20, 0x80, 0x00, 0x00, 0xe0, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t busym_bits[] = {
    0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00,
    0x0f, 0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00, 0x3f, 0x00, 0x00, 0x00,
    0x7f, 0xe0, 0xff, 0x00, 0xff, 0xe0, 0xff, 0x00, 0xff, 0xe1, 0xff, 0x00,
    0xff, 0xc3, 0x7f, 0x00, 0xff, 0xc7, 0x7f, 0x00, 0x7f, 0xc0, 0x7f, 0x00,
    0xf7, 0x80, 0x3f, 0x00, 0xf3, 0x00, 0x1f, 0x00, 0xe1, 0x01, 0x0e, 0x00,
    0xe0, 0x01, 0x1f, 0x00, 0xc0, 0x83, 0x3f, 0x00, 0xc0, 0xc3, 0x7f, 0x00,
    0x80, 0xc1, 0x7f, 0x00, 0x00, 0xc0, 0x7f, 0x00, 0x00, 0xe0, 0xff, 0x00,
    0x00, 0xe0, 0xff, 0x00, 0x00, 0xe0, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static const uint8_t * const cursor_bits32[] = {
    vsplit_bits, vsplitm_bits, hsplit_bits, hsplitm_bits,
    0, 0, 0, 0, whatsthis_bits, whatsthism_bits, busy_bits, busym_bits
};

static const uint8_t forbidden_bits[] = {
    0x00,0x00,0x00,0x80,0x1f,0x00,0xe0,0x7f,0x00,0xf0,0xf0,0x00,0x38,0xc0,0x01,
    0x7c,0x80,0x03,0xec,0x00,0x03,0xce,0x01,0x07,0x86,0x03,0x06,0x06,0x07,0x06,
    0x06,0x0e,0x06,0x06,0x1c,0x06,0x0e,0x38,0x07,0x0c,0x70,0x03,0x1c,0xe0,0x03,
    0x38,0xc0,0x01,0xf0,0xe0,0x00,0xe0,0x7f,0x00,0x80,0x1f,0x00,0x00,0x00,0x00 };

static const uint8_t forbiddenm_bits[] = {
    0x80,0x1f,0x00,0xe0,0x7f,0x00,0xf0,0xff,0x00,0xf8,0xff,0x01,0xfc,0xf0,0x03,
    0xfe,0xc0,0x07,0xfe,0x81,0x07,0xff,0x83,0x0f,0xcf,0x07,0x0f,0x8f,0x0f,0x0f,
    0x0f,0x1f,0x0f,0x0f,0x3e,0x0f,0x1f,0xfc,0x0f,0x1e,0xf8,0x07,0x3e,0xf0,0x07,
    0xfc,0xe0,0x03,0xf8,0xff,0x01,0xf0,0xff,0x00,0xe0,0x7f,0x00,0x80,0x1f,0x00};

static const uint8_t openhand_bits[] = {
    0x80,0x01,0x58,0x0e,0x64,0x12,0x64,0x52,0x48,0xb2,0x48,0x92,
    0x16,0x90,0x19,0x80,0x11,0x40,0x02,0x40,0x04,0x40,0x04,0x20,
    0x08,0x20,0x10,0x10,0x20,0x10,0x00,0x00};
static const uint8_t openhandm_bits[] = {
    0x80,0x01,0xd8,0x0f,0xfc,0x1f,0xfc,0x5f,0xf8,0xff,0xf8,0xff,
    0xf6,0xff,0xff,0xff,0xff,0x7f,0xfe,0x7f,0xfc,0x7f,0xfc,0x3f,
    0xf8,0x3f,0xf0,0x1f,0xe0,0x1f,0x00,0x00};
static const uint8_t closedhand_bits[] = {
    0x00,0x00,0x00,0x00,0x00,0x00,0xb0,0x0d,0x48,0x32,0x08,0x50,
    0x10,0x40,0x18,0x40,0x04,0x40,0x04,0x20,0x08,0x20,0x10,0x10,
    0x20,0x10,0x20,0x10,0x00,0x00,0x00,0x00};
static const uint8_t closedhandm_bits[] = {
    0x00,0x00,0x00,0x00,0x00,0x00,0xb0,0x0d,0xf8,0x3f,0xf8,0x7f,
    0xf0,0x7f,0xf8,0x7f,0xfc,0x7f,0xfc,0x3f,0xf8,0x3f,0xf0,0x1f,
    0xe0,0x1f,0xe0,0x1f,0x00,0x00,0x00,0x00};

static const uint8_t * const cursor_bits20[] = {
    forbidden_bits, forbiddenm_bits
};

static const char * const cursorNames[] = {
    "left_ptr",
    "up_arrow",
    "cross",
    "wait",
    "ibeam",
    "size_ver",
    "size_hor",
    "size_bdiag",
    "size_fdiag",
    "size_all",
    "blank",
    "split_v",
    "split_h",
    "pointing_hand",
    "forbidden",
    "whats_this",
    "left_ptr_watch",
    "openhand",
    "closedhand",
    "copy",
    "move",
    "link"
};

QXcbCursorCacheKey::QXcbCursorCacheKey(const QCursor &c)
    : shape(c.shape()), bitmapCacheKey(0), maskCacheKey(0)
{
    if (shape == Qt::BitmapCursor) {
        const qint64 pixmapCacheKey = c.pixmap().cacheKey();
        if (pixmapCacheKey) {
            bitmapCacheKey = pixmapCacheKey;
        } else {
            Q_ASSERT(c.bitmap());
            Q_ASSERT(c.mask());
            bitmapCacheKey = c.bitmap()->cacheKey();
            maskCacheKey = c.mask()->cacheKey();
        }
    }
}

#endif // !QT_NO_CURSOR

QXcbCursor::QXcbCursor(QXcbConnection *conn, QXcbScreen *screen)
    : QXcbObject(conn), m_screen(screen), m_gtkCursorThemeInitialized(false)
{
    if (cursorCount++)
        return;

    cursorFont = xcb_generate_id(xcb_connection());
    const char *cursorStr = "cursor";
    xcb_open_font(xcb_connection(), cursorFont, strlen(cursorStr), cursorStr);

#if defined(XCB_USE_XLIB) && QT_CONFIG(library)
    static bool function_ptrs_not_initialized = true;
    if (function_ptrs_not_initialized) {
        QLibrary xcursorLib(QLatin1String("Xcursor"), 1);
        bool xcursorFound = xcursorLib.load();
        if (!xcursorFound) { // try without the version number
            xcursorLib.setFileName(QLatin1String("Xcursor"));
            xcursorFound = xcursorLib.load();
        }
        if (xcursorFound) {
            ptrXcursorLibraryLoadCursor =
                (PtrXcursorLibraryLoadCursor) xcursorLib.resolve("XcursorLibraryLoadCursor");
            ptrXcursorLibraryGetTheme =
                    (PtrXcursorLibraryGetTheme) xcursorLib.resolve("XcursorGetTheme");
            ptrXcursorLibrarySetTheme =
                    (PtrXcursorLibrarySetTheme) xcursorLib.resolve("XcursorSetTheme");
            ptrXcursorLibraryGetDefaultSize =
                    (PtrXcursorLibraryGetDefaultSize) xcursorLib.resolve("XcursorGetDefaultSize");
        }
        function_ptrs_not_initialized = false;
    }

#endif
}

QXcbCursor::~QXcbCursor()
{
    xcb_connection_t *conn = xcb_connection();

    if (m_gtkCursorThemeInitialized) {
        m_screen->xSettings()->removeCallbackForHandle(this);
    }

    if (!--cursorCount)
        xcb_close_font(conn, cursorFont);

#ifndef QT_NO_CURSOR
    for (xcb_cursor_t cursor : qAsConst(m_cursorHash))
        xcb_free_cursor(conn, cursor);
#endif
}

#ifndef QT_NO_CURSOR
void QXcbCursor::changeCursor(QCursor *cursor, QWindow *widget)
{
    QXcbWindow *w = 0;
    if (widget && widget->handle())
        w = static_cast<QXcbWindow *>(widget->handle());
    else
        // No X11 cursor control when there is no widget under the cursor
        return;

    xcb_cursor_t c = XCB_CURSOR_NONE;
    bool isBitmapCursor = false;

    if (cursor) {
        const Qt::CursorShape shape = cursor->shape();
        isBitmapCursor = shape == Qt::BitmapCursor;

        if (!isBitmapCursor) {
            const QXcbCursorCacheKey key(*cursor);
            CursorHash::iterator it = m_cursorHash.find(key);
            if (it == m_cursorHash.end()) {
                it = m_cursorHash.insert(key, createFontCursor(shape));
            }
            c = it.value();
        } else {
            // Do not cache bitmap cursors, as otherwise they have unclear
            // lifetime (we effectively leak xcb_cursor_t).
            c = createBitmapCursor(cursor);
        }
    }

    w->setCursor(c, isBitmapCursor);
}

static int cursorIdForShape(int cshape)
{
    int cursorId = 0;
    switch (cshape) {
    case Qt::ArrowCursor:
        cursorId = XC_left_ptr;
        break;
    case Qt::UpArrowCursor:
        cursorId = XC_center_ptr;
        break;
    case Qt::CrossCursor:
        cursorId = XC_crosshair;
        break;
    case Qt::WaitCursor:
        cursorId = XC_watch;
        break;
    case Qt::IBeamCursor:
        cursorId = XC_xterm;
        break;
    case Qt::SizeAllCursor:
        cursorId = XC_fleur;
        break;
    case Qt::PointingHandCursor:
        cursorId = XC_hand2;
        break;
    case Qt::SizeBDiagCursor:
        cursorId = XC_top_right_corner;
        break;
    case Qt::SizeFDiagCursor:
        cursorId = XC_bottom_right_corner;
        break;
    case Qt::SizeVerCursor:
    case Qt::SplitVCursor:
        cursorId = XC_sb_v_double_arrow;
        break;
    case Qt::SizeHorCursor:
    case Qt::SplitHCursor:
        cursorId = XC_sb_h_double_arrow;
        break;
    case Qt::WhatsThisCursor:
        cursorId = XC_question_arrow;
        break;
    case Qt::ForbiddenCursor:
        cursorId = XC_circle;
        break;
    case Qt::BusyCursor:
        cursorId = XC_watch;
        break;
    default:
        break;
    }
    return cursorId;
}

xcb_cursor_t QXcbCursor::createNonStandardCursor(int cshape)
{
    xcb_cursor_t cursor = 0;
    xcb_connection_t *conn = xcb_connection();

    if (cshape == Qt::BlankCursor) {
        xcb_pixmap_t cp = xcb_create_pixmap_from_bitmap_data(conn, m_screen->root(), cur_blank_bits, 16, 16,
                                                             1, 0, 0, 0);
        xcb_pixmap_t mp = xcb_create_pixmap_from_bitmap_data(conn, m_screen->root(), cur_blank_bits, 16, 16,
                                                             1, 0, 0, 0);
        cursor = xcb_generate_id(conn);
        xcb_create_cursor(conn, cursor, cp, mp, 0, 0, 0, 0xFFFF, 0xFFFF, 0xFFFF, 8, 8);
    } else if (cshape >= Qt::SizeVerCursor && cshape < Qt::SizeAllCursor) {
        int i = (cshape - Qt::SizeVerCursor) * 2;
        xcb_pixmap_t pm = xcb_create_pixmap_from_bitmap_data(conn, m_screen->root(),
                                                             const_cast<uint8_t*>(cursor_bits16[i]),
                                                             16, 16, 1, 0, 0, 0);
        xcb_pixmap_t pmm = xcb_create_pixmap_from_bitmap_data(conn, m_screen->root(),
                                                              const_cast<uint8_t*>(cursor_bits16[i + 1]),
                                                              16, 16, 1, 0, 0, 0);
        cursor = xcb_generate_id(conn);
        xcb_create_cursor(conn, cursor, pm, pmm, 0, 0, 0, 0xFFFF, 0xFFFF, 0xFFFF, 8, 8);
    } else if ((cshape >= Qt::SplitVCursor && cshape <= Qt::SplitHCursor)
               || cshape == Qt::WhatsThisCursor || cshape == Qt::BusyCursor) {
        int i = (cshape - Qt::SplitVCursor) * 2;
        xcb_pixmap_t pm = xcb_create_pixmap_from_bitmap_data(conn, m_screen->root(),
                                                             const_cast<uint8_t*>(cursor_bits32[i]),
                                                             32, 32, 1, 0, 0, 0);
        xcb_pixmap_t pmm = xcb_create_pixmap_from_bitmap_data(conn, m_screen->root(),
                                                              const_cast<uint8_t*>(cursor_bits32[i + 1]),
                                                              32, 32, 1, 0, 0, 0);
        int hs = (cshape == Qt::PointingHandCursor || cshape == Qt::WhatsThisCursor
                  || cshape == Qt::BusyCursor) ? 0 : 16;
        cursor = xcb_generate_id(conn);
        xcb_create_cursor(conn, cursor, pm, pmm, 0, 0, 0, 0xFFFF, 0xFFFF, 0xFFFF, hs, hs);
    } else if (cshape == Qt::ForbiddenCursor) {
        int i = (cshape - Qt::ForbiddenCursor) * 2;
        xcb_pixmap_t pm = xcb_create_pixmap_from_bitmap_data(conn, m_screen->root(),
                                                             const_cast<uint8_t*>(cursor_bits20[i]),
                                                             20, 20, 1, 0, 0, 0);
        xcb_pixmap_t pmm = xcb_create_pixmap_from_bitmap_data(conn, m_screen->root(),
                                                              const_cast<uint8_t*>(cursor_bits20[i + 1]),
                                                              20, 20, 1, 0, 0, 0);
        cursor = xcb_generate_id(conn);
        xcb_create_cursor(conn, cursor, pm, pmm, 0, 0, 0, 0xFFFF, 0xFFFF, 0xFFFF, 10, 10);
    } else if (cshape == Qt::OpenHandCursor || cshape == Qt::ClosedHandCursor) {
        bool open = cshape == Qt::OpenHandCursor;
        xcb_pixmap_t pm = xcb_create_pixmap_from_bitmap_data(conn, m_screen->root(),
                                                             const_cast<uint8_t*>(open ? openhand_bits : closedhand_bits),
                                                             16, 16, 1, 0, 0, 0);
        xcb_pixmap_t pmm = xcb_create_pixmap_from_bitmap_data(conn, m_screen->root(),
                                                             const_cast<uint8_t*>(open ? openhandm_bits : closedhandm_bits),
                                                             16, 16, 1, 0, 0, 0);
        cursor = xcb_generate_id(conn);
        xcb_create_cursor(conn, cursor, pm, pmm, 0, 0, 0, 0xFFFF, 0xFFFF, 0xFFFF, 8, 8);
    } else if (cshape == Qt::DragCopyCursor || cshape == Qt::DragMoveCursor
               || cshape == Qt::DragLinkCursor) {
        QImage image = QGuiApplicationPrivate::instance()->getPixmapCursor(static_cast<Qt::CursorShape>(cshape)).toImage();
        if (!image.isNull()) {
            xcb_pixmap_t pm = qt_xcb_XPixmapFromBitmap(m_screen, image);
            xcb_pixmap_t pmm = qt_xcb_XPixmapFromBitmap(m_screen, image.createAlphaMask());
            cursor = xcb_generate_id(conn);
            xcb_create_cursor(conn, cursor, pm, pmm, 0, 0, 0, 0xFFFF, 0xFFFF, 0xFFFF, 8, 8);
            xcb_free_pixmap(conn, pm);
            xcb_free_pixmap(conn, pmm);
        }
    }

    return cursor;
}

#if defined(XCB_USE_XLIB) && QT_CONFIG(library)
bool updateCursorTheme(void *dpy, const QByteArray &theme) {
    if (!ptrXcursorLibraryGetTheme
            || !ptrXcursorLibrarySetTheme)
        return false;
    QByteArray oldTheme = ptrXcursorLibraryGetTheme(dpy);
    if (oldTheme == theme)
        return false;

    int setTheme = ptrXcursorLibrarySetTheme(dpy,theme.constData());
    return setTheme;
}

 void QXcbCursor::cursorThemePropertyChanged(QXcbVirtualDesktop *screen, const QByteArray &name, const QVariant &property, void *handle)
{
    Q_UNUSED(screen);
    Q_UNUSED(name);
    QXcbCursor *self = static_cast<QXcbCursor *>(handle);
    updateCursorTheme(self->connection()->xlib_display(),property.toByteArray());
}

static xcb_cursor_t loadCursor(void *dpy, int cshape)
{
    xcb_cursor_t cursor = XCB_NONE;
    if (!ptrXcursorLibraryLoadCursor || !dpy)
        return cursor;
    switch (cshape) {
    case Qt::DragCopyCursor:
        cursor = ptrXcursorLibraryLoadCursor(dpy, "dnd-copy");
        break;
    case Qt::DragMoveCursor:
        cursor = ptrXcursorLibraryLoadCursor(dpy, "dnd-move");
        break;
    case Qt::DragLinkCursor:
        cursor = ptrXcursorLibraryLoadCursor(dpy, "dnd-link");
        break;
    default:
        break;
    }
    if (!cursor) {
        cursor = ptrXcursorLibraryLoadCursor(dpy, cursorNames[cshape]);
    }
    return cursor;
}
#endif // XCB_USE_XLIB / QT_CONFIG(library)

xcb_cursor_t QXcbCursor::createFontCursor(int cshape)
{
    xcb_connection_t *conn = xcb_connection();
    int cursorId = cursorIdForShape(cshape);
    xcb_cursor_t cursor = XCB_NONE;

    // Try Xcursor first
#if defined(XCB_USE_XLIB) && QT_CONFIG(library)
    if (cshape >= 0 && cshape <= Qt::LastCursor) {
        void *dpy = connection()->xlib_display();
        // special case for non-standard dnd-* cursors
        cursor = loadCursor(dpy, cshape);
        if (!cursor && !m_gtkCursorThemeInitialized && m_screen->xSettings()->initialized()) {
            QByteArray gtkCursorTheme = m_screen->xSettings()->setting("Gtk/CursorThemeName").toByteArray();
            m_screen->xSettings()->registerCallbackForProperty("Gtk/CursorThemeName",cursorThemePropertyChanged,this);
            if (updateCursorTheme(dpy,gtkCursorTheme)) {
                cursor = loadCursor(dpy, cshape);
            }
            m_gtkCursorThemeInitialized = true;
        }
    }
    if (cursor)
        return cursor;
    if (!cursor && cursorId) {
        cursor = XCreateFontCursor(DISPLAY_FROM_XCB(this), cursorId);
        if (cursor)
            return cursor;
    }

#endif

    // Non-standard X11 cursors are created from bitmaps
    cursor = createNonStandardCursor(cshape);

    // Create a glpyh cursor if everything else failed
    if (!cursor && cursorId) {
        cursor = xcb_generate_id(conn);
        xcb_create_glyph_cursor(conn, cursor, cursorFont, cursorFont,
                                cursorId, cursorId + 1,
                                0xFFFF, 0xFFFF, 0xFFFF, 0, 0, 0);
    }

    if (cursor && cshape >= 0 && cshape < Qt::LastCursor && connection()->hasXFixes()) {
        const char *name = cursorNames[cshape];
        xcb_xfixes_set_cursor_name(conn, cursor, strlen(name), name);
    }

    return cursor;
}

xcb_cursor_t QXcbCursor::createBitmapCursor(QCursor *cursor)
{
    xcb_connection_t *conn = xcb_connection();
    QPoint spot = cursor->hotSpot();
    xcb_cursor_t c = XCB_NONE;
    if (cursor->pixmap().depth() > 1)
        c = qt_xcb_createCursorXRender(m_screen, cursor->pixmap().toImage(), spot);
    if (!c) {
        xcb_pixmap_t cp = qt_xcb_XPixmapFromBitmap(m_screen, cursor->bitmap()->toImage());
        xcb_pixmap_t mp = qt_xcb_XPixmapFromBitmap(m_screen, cursor->mask()->toImage());
        c = xcb_generate_id(conn);
        xcb_create_cursor(conn, c, cp, mp, 0, 0, 0, 0xFFFF, 0xFFFF, 0xFFFF,
                          spot.x(), spot.y());
        xcb_free_pixmap(conn, cp);
        xcb_free_pixmap(conn, mp);
    }
    return c;
}
#endif

void QXcbCursor::queryPointer(QXcbConnection *c, QXcbVirtualDesktop **virtualDesktop, QPoint *pos, int *keybMask)
{
    if (pos)
        *pos = QPoint();

    xcb_window_t root = c->primaryVirtualDesktop()->root();
    xcb_query_pointer_cookie_t cookie = xcb_query_pointer(c->xcb_connection(), root);
    xcb_generic_error_t *err = 0;
    xcb_query_pointer_reply_t *reply = xcb_query_pointer_reply(c->xcb_connection(), cookie, &err);
    if (!err && reply) {
        if (virtualDesktop) {
            const auto virtualDesktops = c->virtualDesktops();
            for (QXcbVirtualDesktop *vd : virtualDesktops) {
                if (vd->root() == reply->root) {
                    *virtualDesktop = vd;
                    break;
                }
            }
        }
        if (pos)
            *pos = QPoint(reply->root_x, reply->root_y);
        if (keybMask)
            *keybMask = reply->mask;
        free(reply);
        return;
    }
    free(err);
    free(reply);
}

QPoint QXcbCursor::pos() const
{
    QPoint p;
    queryPointer(connection(), 0, &p);
    return p;
}

void QXcbCursor::setPos(const QPoint &pos)
{
    QXcbVirtualDesktop *virtualDesktop = Q_NULLPTR;
    queryPointer(connection(), &virtualDesktop, 0);
    xcb_warp_pointer(xcb_connection(), XCB_NONE, virtualDesktop->root(), 0, 0, 0, 0, pos.x(), pos.y());
    xcb_flush(xcb_connection());
}

QT_END_NAMESPACE
