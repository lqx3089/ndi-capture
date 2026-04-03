set(SOURCES
    main.cpp
    Logger.cpp
    MutexGuard.cpp
    DeviceEnumerator.cpp
    FrameConverter.cpp
    MjpegDecoder.cpp
    NdiSender.cpp
    CaptureSession.cpp
    MainWindow.cpp
)

set(HEADERS
    Logger.h
    MutexGuard.h
    DeviceEnumerator.h
    FrameConverter.h
    MjpegDecoder.h
    NdiSenderInterface.h
    NdiSender.h
    CaptureSession.h
    MainWindow.h
)

qt_add_executable(NDICapture WIN32
    ${SOURCES}
    ${HEADERS}
)

# ── Include dirs ────────────────────────────────────────────────────────────
target_include_directories(NDICapture PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# ── Qt ──────────────────────────────────────────────────────────────────────
target_link_libraries(NDICapture PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

# ── libyuv ─────────────────────────────────────────────────────────────────
target_link_libraries(NDICapture PRIVATE libyuv::libyuv)
target_compile_definitions(NDICapture PRIVATE HAVE_LIBYUV)

# ── libjpeg-turbo ───────────────────────────────────────────────────────────
target_link_libraries(NDICapture PRIVATE libjpeg-turbo::turbojpeg)
target_compile_definitions(NDICapture PRIVATE HAVE_LIBJPEG_TURBO)

# ── NDI (optional) ──────────────────────────────────────────────────────────
if(WITH_NDI)
    target_link_libraries(NDICapture PRIVATE NDI::NDI)
    target_compile_definitions(NDICapture PRIVATE HAVE_NDI)
endif()

# ── Windows-specific ────────────────────────────────────────────────────────
if(WIN32)
    target_link_libraries(NDICapture PRIVATE
        strmiids       # DirectShow
        ole32
        oleaut32
        quartz
    )
    # Unicode
    target_compile_definitions(NDICapture PRIVATE
        UNICODE _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    # Ensure logs/ directory exists in build output
    add_custom_command(TARGET NDICapture POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:NDICapture>/logs"
        COMMENT "Creating logs directory"
    )
endif()

# ── C++17 ───────────────────────────────────────────────────────────────────
set_target_properties(NDICapture PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
