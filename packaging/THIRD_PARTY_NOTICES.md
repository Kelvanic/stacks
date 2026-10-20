# Third-party software in Stacks packages

Stacks dynamically links Qt. Qt and its use are covered by the GNU Lesser
General Public License version 3 (or the applicable alternative license).
Copies of LGPL 3 and GPL 3 accompany this notice. Qt source and build instructions:
https://download.qt.io/archive/qt/6.8/6.8.3/ and https://code.qt.io/qt/.
Interface-compatible modified shared libraries may be substituted; debugging
and reverse engineering for that purpose are not prohibited. Unsigned/ad-hoc
signed packages do not require a publisher key to rebuild/re-sign modifications.

Qt Multimedia may deploy FFmpeg libraries, covered by LGPL 2.1 or later; that
license is also included. See https://ffmpeg.org/legal.html and the FFmpeg source
references in https://code.qt.io/cgit/qt/qtmultimedia.git/tree/src/3rdparty?h=v6.8.3.
Qt also incorporates third-party libraries; their attributions and licenses are
documented at https://doc.qt.io/qt-6/licenses-used-in-qt.html.

PDFium and its dependencies are covered by the licenses under `pdfium/`.
The build uses the unmodified chromium/8111 PDFium binary distribution from
https://github.com/bblanchon/pdfium-binaries/releases/tag/chromium%2F8111;
its source and dependency information are published by that project.

libzip is BSD licensed and zlib uses the zlib license; their notices are included.
Additional vcpkg dependency copyright files are copied into Windows packages.
Inter, Newsreader, and JetBrains Mono are covered by their included SIL Open
Font Licenses under `fonts/`.

`BUILD_INFO.json` identifies the exact Stacks commit, Qt version, PDFium release,
and package architecture. Stacks source/build scripts are available at
https://github.com/Kelvanic/stacks. This notice does not assign a new license to
the Stacks application source or to the generated artwork.
