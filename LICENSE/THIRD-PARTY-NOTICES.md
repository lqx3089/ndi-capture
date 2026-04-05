# Third-Party Notices

This file lists third-party components used by NDI Capture and their licenses.

## 1. Qt 6.x

- Project: Qt
- Homepage: https://www.qt.io/
- License used by this project: LGPL (typically LGPL-3.0 for Qt 6 open-source distribution)
- Notes:
  - Qt license texts used for your shipped binaries should be included in this `LICENSE/` directory.
  - See `LICENSE/QT-LICENSE-PLACEHOLDER.txt` for file placement guidance.

## 2. libyuv

- Project: libyuv
- Homepage: https://chromium.googlesource.com/libyuv/libyuv/
- License: BSD 3-Clause (Modified BSD)
- Source license file: https://chromium.googlesource.com/libyuv/libyuv/+/refs/heads/main/LICENSE

## 3. libjpeg-turbo

- Project: libjpeg-turbo
- Homepage: https://libjpeg-turbo.org/
- Licenses: IJG License + BSD 3-Clause (Modified BSD)
- Source references:
  - https://github.com/libjpeg-turbo/libjpeg-turbo/blob/main/LICENSE.md
  - https://github.com/libjpeg-turbo/libjpeg-turbo/blob/main/README.ijg
- Required attribution text for binary distributions:

  This software is based in part on the work of the Independent JPEG Group.

## 4. NDI SDK / NDI Runtime

- Project: NDI SDK / NDI Runtime
- Homepage: https://ndi.video/
- License: Proprietary, under NewTek/Vizrt license terms
- Notes:
  - Not bundled in this repository.
  - Users must download/install it separately.

## Distribution Checklist

When distributing binaries, include at least:

1. `LICENSE/LICENSE`
2. Qt license texts matching the exact shipped Qt binaries
3. This file: `LICENSE/THIRD-PARTY-NOTICES.md`
4. Any additional third-party license texts required by your exact dependency binaries
