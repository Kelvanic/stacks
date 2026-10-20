pragma Singleton
import QtQuick

// Observable semantic tokens: changing appearance updates every bound component
// without reloading the window, the book, or its reading position.
QtObject {
    readonly property bool dark: controller.darkTheme

    readonly property color ink0: dark ? "#0A0908" : "#F2EEE7"
    readonly property color ink1: dark ? "#12100E" : "#FCFAF6"
    readonly property color ink2: dark ? "#1A1714" : "#F0EBE3"
    readonly property color ink3: dark ? "#241F1A" : "#E7DFD3"
    readonly property color ink4: dark ? "#312A22" : "#D8CDBD"

    readonly property color featureSurface: dark ? "#1C302B" : "#293E36"
    readonly property color stage: dark ? "#171612" : "#F1EDE5"
    readonly property color paper: dark ? "#221E19" : "#FFFDF8"
    readonly property color paperEdge: dark ? "#3B3329" : "#E5D9C6"
    readonly property color paperInk: dark ? "#EDE4D5" : "#272119"
    readonly property color paperMuted: dark ? "#B0A593" : "#756B5D"
    readonly property color paperLine: dark ? "#665C4C" : "#45302518"

    readonly property color text: dark ? "#F0E9DB" : "#24201B"
    readonly property color textMuted: dark ? "#B5AC9F" : "#5E574E"
    readonly property color textFaint: dark ? "#A49B8D" : "#6D6257"
    readonly property color textGhost: dark ? "#998F81" : "#73675B"

    readonly property color copper: dark ? "#C9703F" : "#9C4D25"
    readonly property color copperBright: dark ? "#E08B57" : "#B35B2E"
    readonly property color copperSoft: dark ? "#66C9703F" : "#669C4D25"
    readonly property color copperWash: dark ? "#24C9703F" : "#169C4D25"
    readonly property color primaryText: dark ? "#1A130D" : "#FFF8EF"
    readonly property color moss: dark ? "#9AAE84" : "#4B653D"
    readonly property color ochre: dark ? "#D6A866" : "#8C5E20"
    readonly property color violet: dark ? "#B1A0D2" : "#70528E"
    readonly property color rose: dark ? "#DE968D" : "#A04440"

    readonly property color line: dark ? "#24F0E9DB" : "#24302518"
    readonly property color lineSoft: dark ? "#1AF0E9DB" : "#18302518"
    readonly property color lineStrong: dark ? "#45F0E9DB" : "#45302518"
    readonly property color hair: dark ? "#22FFFFFF" : "#CCFFFFFF"
    readonly property color scrim: dark ? "#B3000000" : "#66231B12"
    readonly property color shadow: dark ? "#33000000" : "#18231B12"
    readonly property color deepShadow: dark ? "#55000000" : "#24231B12"
    readonly property color gridDots: dark ? "#24F0E9DB" : "#30302518"
    readonly property string glow: dark ? "rgba(201,112,63,0.13)" : "rgba(156,77,37,0.07)"
    readonly property string glowSoft: dark ? "rgba(160,110,70,0.05)" : "rgba(160,110,70,0.025)"
    readonly property real grainOpacity: dark ? 0.3 : 0.13
    readonly property real paperTextureOpacity: dark ? 0.06 : 0.25

    readonly property string fontUI: "Inter"
    readonly property string fontBook: "Newsreader"
    readonly property string fontMono: "JetBrains Mono"
    readonly property int railLeft: 320
    readonly property int railRight: 348
    readonly property int topBar: 76
}
