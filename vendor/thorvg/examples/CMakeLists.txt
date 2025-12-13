# ============================================================
# ThorVG Examples
# ============================================================
# These examples require SDL2 for window creation and event handling

find_package(SDL2 CONFIG QUIET)

if(NOT SDL2_FOUND)
    message(WARNING "SDL2 not found. Skipping ThorVG examples. Install SDL2 to build examples.")
    return()
endif()

message(STATUS "Building ThorVG examples with SDL2")

# Define resource directory
set(EXAMPLE_RESOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/resources")

# ============================================================
# Example Sources
# ============================================================
set(EXAMPLE_SOURCES
    Accessor.cpp
    Animation.cpp
    Blending.cpp
    BoundingBox.cpp
    Clipping.cpp
    CustomTransform.cpp
    DataLoad.cpp
    DirectUpdate.cpp
    Duplicate.cpp
    EffectDropShadow.cpp
    SceneEffects.cpp
    FillRule.cpp
    FillSpread.cpp
    GradientMasking.cpp
    GradientStroke.cpp
    GradientTransform.cpp
    ImageRotation.cpp
    ImageScaling.cpp
    Intersects.cpp
    LinearGradient.cpp
    Lottie.cpp
    LumaMasking.cpp
    Masking.cpp
    MaskingMethods.cpp
    MultiCanvas.cpp
    Opacity.cpp
    Path.cpp
    Particles.cpp
    PictureJpg.cpp
    PicturePng.cpp
    PictureRaw.cpp
    PictureSvg.cpp
    PictureWebp.cpp
    RadialGradient.cpp
    Retaining.cpp
    Scene.cpp
    SceneBlending.cpp
    SceneTransform.cpp
    Shapes.cpp
    Stroke.cpp
    StrokeLine.cpp
    StrokeMiterlimit.cpp
    Svg.cpp
    Text.cpp
    TextLayout.cpp
    TextLineWrap.cpp
    Transform.cpp
    TrimPath.cpp
    Update.cpp
    Viewport.cpp
)

# Optional examples based on enabled features
if(THORVG_SAVER_GIF)
    list(APPEND EXAMPLE_SOURCES GifSaver.cpp)
endif()

if(THORVG_LOADER_LOTTIE)
    list(APPEND EXAMPLE_SOURCES
        LottieExtension.cpp
        LottieTweening.cpp
    )
endif()

if(THORVG_LOTTIE_EXPRESSIONS_SUPPORT)
    list(APPEND EXAMPLE_SOURCES
        LottieExpressions.cpp
        LottieInteraction.cpp
    )
endif()

# ============================================================
# Compiler Flags for Examples
# ============================================================
set(EXAMPLE_COMPILE_FLAGS)

if(THORVG_BUILD_SHARED)
    # Shared library mode
else()
    list(APPEND EXAMPLE_COMPILE_FLAGS -DTVG_STATIC)
endif()

if(THORVG_GL_USE_GLES)
    list(APPEND EXAMPLE_COMPILE_FLAGS -DTHORVG_GL_TARGET_GLES=1)
endif()

list(APPEND EXAMPLE_COMPILE_FLAGS -DSDL_MAIN_HANDLED)

# ============================================================
# Create Example Executables
# ============================================================
foreach(EXAMPLE_FILE ${EXAMPLE_SOURCES})
    get_filename_component(EXAMPLE_NAME ${EXAMPLE_FILE} NAME_WE)

    add_executable(${EXAMPLE_NAME} ${EXAMPLE_FILE})

    target_include_directories(${EXAMPLE_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../inc
        ${CMAKE_CURRENT_BINARY_DIR}/..
    )

    target_link_libraries(${EXAMPLE_NAME} PRIVATE
        thorvg
        $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
        $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
    )

    target_compile_options(${EXAMPLE_NAME} PRIVATE ${EXAMPLE_COMPILE_FLAGS})

    # Define EXAMPLE_DIR macro
    target_compile_definitions(${EXAMPLE_NAME} PRIVATE
        EXAMPLE_DIR="${EXAMPLE_RESOURCE_DIR}"
    )

    # Set IDE folder
    set_target_properties(${EXAMPLE_NAME} PROPERTIES
        FOLDER "vendor/thorvg/examples"
    )
endforeach()

# ============================================================
# CAPI Examples (if enabled)
# ============================================================
if(THORVG_BINDINGS_CAPI)
    add_executable(Capi Capi.cpp)

    target_include_directories(Capi PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../inc
        ${CMAKE_CURRENT_BINARY_DIR}/..
    )

    target_link_libraries(Capi PRIVATE
        thorvg
        $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
        $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
    )

    target_compile_options(Capi PRIVATE ${EXAMPLE_COMPILE_FLAGS})

    # Define EXAMPLE_DIR macro
    target_compile_definitions(Capi PRIVATE
        EXAMPLE_DIR="${EXAMPLE_RESOURCE_DIR}"
    )

    # Set IDE folder
    set_target_properties(Capi PROPERTIES
        FOLDER "vendor/thorvg/examples"
    )
endif()

message(STATUS "ThorVG: ${CMAKE_CURRENT_LIST_FILE} created ${CMAKE_MATCH_COUNT} example executables")
