TARGET = Qt$${QT_MAJOR_VERSION}Android

CONFIG += java

DESTDIR = $$[QT_INSTALL_PREFIX/get]/jar

PATHPREFIX = $$PWD/src/org/qtproject/qt/android/

JAVACLASSPATH += $$PWD/src/
JAVASOURCES += \
    $$PATHPREFIX/accessibility/QtAccessibilityDelegate.java \
    $$PATHPREFIX/accessibility/QtNativeAccessibility.java \
    $$PATHPREFIX/QtActivityDelegate.java \
    $$PATHPREFIX/QtEditText.java \
    $$PATHPREFIX/QtInputConnection.java \
    $$PATHPREFIX/QtLayout.java \
    $$PATHPREFIX/QtMessageDialogHelper.java \
    $$PATHPREFIX/QtNative.java \
    $$PATHPREFIX/QtNativeLibrariesDir.java \
    $$PATHPREFIX/QtSurface.java \
    $$PATHPREFIX/ExtractStyle.java \
    $$PATHPREFIX/EditContextView.java \
    $$PATHPREFIX/EditPopupMenu.java \
    $$PATHPREFIX/CursorHandle.java \
    $$PATHPREFIX/QtThread.java

# install
target.path = $$[QT_INSTALL_PREFIX]/jar
INSTALLS += target
