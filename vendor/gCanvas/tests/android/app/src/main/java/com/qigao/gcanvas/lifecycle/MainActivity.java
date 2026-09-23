package com.qigao.gcanvas.lifecycle;

import android.app.Activity;
import android.graphics.PixelFormat;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.widget.FrameLayout;

import java.io.FileOutputStream;
import java.nio.charset.StandardCharsets;

public final class MainActivity extends Activity implements SurfaceHolder.Callback {
    private static final String TAG = "GCANVAS_ANDROID_TEST";

    static {
        System.loadLibrary("gcanvas_android_lifecycle_test");
    }

    private final Handler handler = new Handler(Looper.getMainLooper());
    private FrameLayout root;
    private SurfaceView surfaceView;
    private boolean recreateRequested;
    private boolean resultWritten;

    private static native int nativeSurfaceCreated(Surface surface);
    private static native void nativeSurfaceDestroyed();
    private static native void nativeDestroy();
    private static native String nativeFailure();

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        root = new FrameLayout(this);
        setContentView(root);
        addSurface();
    }

    private void addSurface() {
        SurfaceView view = new SurfaceView(this);
        view.getHolder().setFormat(PixelFormat.RGBA_8888);
        view.getHolder().addCallback(this);
        surfaceView = view;
        root.addView(view, new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT));
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        final int stage = nativeSurfaceCreated(holder.getSurface());
        if (stage < 0) {
            fail(nativeFailure());
            return;
        }

        if (stage == 1) {
            recreateRequested = true;
            handler.postDelayed(() -> {
                SurfaceView current = surfaceView;
                surfaceView = null;
                if (current != null) {
                    root.removeView(current);
                }
            }, 250L);
        } else if (stage == 2) {
            pass();
        } else {
            fail("unexpected native stage " + stage);
        }
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        nativeSurfaceDestroyed();
        if (recreateRequested) {
            recreateRequested = false;
            handler.postDelayed(this::addSurface, 150L);
        }
    }

    private void pass() {
        if (resultWritten) {
            return;
        }
        resultWritten = true;
        writeResult("PASS");
        Log.i(TAG, "PASS: same gCanvas context and texture survived Surface replacement");
    }

    private void fail(String message) {
        if (resultWritten) {
            return;
        }
        resultWritten = true;
        final String value = "FAIL: " + message;
        writeResult(value);
        Log.e(TAG, value);
    }

    private void writeResult(String value) {
        try (FileOutputStream output =
                     openFileOutput("result.txt", MODE_PRIVATE)) {
            output.write(value.getBytes(StandardCharsets.UTF_8));
        } catch (Exception error) {
            Log.e(TAG, "could not write result", error);
        }
    }

    @Override
    protected void onDestroy() {
        nativeDestroy();
        super.onDestroy();
    }
}
