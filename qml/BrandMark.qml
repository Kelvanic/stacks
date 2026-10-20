import QtQuick

// Use a QML-relative URL so the brand also loads in isolated component views.
Image {
    // Decode at a power of two at or above what the screen needs. Tying the
    // decode size to the width re-decoded the full artwork, and cached
    // another copy, at every step of a window resize.
    readonly property int decodeSize: {
        const needed = Math.max(width, height) * Screen.devicePixelRatio
        let size = 64
        while (size < needed && size < 1024)
            size *= 2
        return size
    }
    source: Qt.resolvedUrl("../assets/branding/stacks-icon.png")
    fillMode: Image.PreserveAspectFit
    mipmap: true
    smooth: true
    sourceSize: Qt.size(decodeSize, decodeSize)
}
