#pragma once
// dshow_guids.h – Centralized fallback GUID definitions for DirectShow media subtypes.
//
// WHY THIS FILE EXISTS
// --------------------
// Modern Windows SDKs (10.0.19041+) define MEDIASUBTYPE_NV12, MEDIASUBTYPE_IYUV, and
// MEDIASUBTYPE_MJPG in strmiids.lib and declare them via DEFINE_GUID in <uuids.h>
// (pulled in transitively by <dshow.h>).  Defining those symbols again as
// `static const GUID` in multiple translation units causes LNK2005/LNK1169 with newer
// toolchains (Visual Studio 2022/2026 + recent Windows SDK).
//
// The GUIDs that the SDK may NOT always provide (e.g. MEDIASUBTYPE_I420) are defined
// here as C++17 `inline const` variables so there is exactly one canonical definition
// regardless of how many translation units include this header.

#ifdef _WIN32
#  include <dshow.h>

// MEDIASUBTYPE_I420  {30323449-0000-0010-8000-00AA00389B71}
// I420 (planar YUV 4:2:0) is not always present in strmiids.lib / uuids.h.
// The inline keyword (C++17) ensures a single definition across all TUs.
#ifndef MEDIASUBTYPE_I420
inline const GUID MEDIASUBTYPE_I420 =
{ 0x30323449, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71 } };
#endif

// MEDIASUBTYPE_HDYC  {43594448-0000-0010-8000-00AA00389B71}
// HDYC is used by Blackmagic Design and AJA capture cards.  It has the same
// UYVY byte layout as MEDIASUBTYPE_UYVY but signals BT.709 colour space.
// It is not present in strmiids.lib so we define it here.
#ifndef MEDIASUBTYPE_HDYC
inline const GUID MEDIASUBTYPE_HDYC =
{ 0x43594448, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71 } };
#endif

#endif // _WIN32
