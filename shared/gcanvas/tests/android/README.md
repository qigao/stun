# Android GLES lifecycle qualification

This is a self-testing Android application for the gCanvas OpenGLES + AndroidEGL path.

The app intentionally keeps one native `gCanvas::Context` and one uploaded 1x1 texture alive
while Java removes the first `SurfaceView` and creates a replacement `SurfaceView`.

The native sequence is:

1. create `AndroidEglHost` and `gCanvas::OpenGLES`
2. upload a persistent green texture
3. render/read back a red first frame
4. `surfaceDestroyed` -> `AndroidEglHost::suspend(context)`
5. create a replacement Android Surface
6. `AndroidEglHost::resume(context, new_window)`
7. draw the same previously uploaded texture, plus text and analytic primitives
8. read back a green center pixel and report PASS

No gCanvas Context, shader program, font atlas, or test texture is recreated between steps 4 and 7.

FreeType 2.14.3 is fetched at native configure time for the test application. The small local
`stb_image.h` shim intentionally disables file image loading because this qualification uses only
memory-backed image creation.
