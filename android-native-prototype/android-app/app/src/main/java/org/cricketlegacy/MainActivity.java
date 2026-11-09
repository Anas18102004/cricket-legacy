package org.cricketlegacy;

import android.app.Activity;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.WindowManager;

public class MainActivity extends Activity implements SurfaceHolder.Callback, View.OnTouchListener {
    private static final String TAG = "CricketLegacyApp";

    static {
        try {
            System.loadLibrary("wgpu_native");
            Log.i(TAG, "Successfully loaded native library: libwgpu_native.so");
        } catch (UnsatisfiedLinkError e) {
            Log.w(TAG, "libwgpu_native.so direct load warning (linker will resolve automatically): " + e.getMessage());
        }
        try {
            System.loadLibrary("cricket_native_harness");
            Log.i(TAG, "Successfully loaded native library: libcricket_native_harness.so");
        } catch (UnsatisfiedLinkError e) {
            Log.e(TAG, "FATAL: Failed to load libcricket_native_harness.so: " + e.getMessage());
        }
    }

    // Native JNI Declarations
    public static native boolean nativeCricketHarnessInit();
    public static native boolean nativeCricketSetSurface(Surface surface);
    public static native void nativeCricketResize(int width, int height);
    public static native void nativeCricketSurfaceDestroyed();
    public static native void nativeCricketOnTouch(int action, float x, float y, int pointerId);
    public static native void nativeCricketRenderFrame();
    public static native boolean nativeCricketAudioTest();
    public static native void nativeCricketHarnessShutdown();

    private SurfaceView mSurfaceView;
    private Handler mHandler;
    private boolean mIsRunning = false;

    private final Runnable mRenderLoop = new Runnable() {
        @Override
        public void run() {
            if (mIsRunning) {
                nativeCricketRenderFrame();
                mHandler.postDelayed(this, 16); // ~60 FPS update pacing
            }
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Keep screen on, full landscape immersive
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN, WindowManager.LayoutParams.FLAG_FULLSCREEN);

        Log.i(TAG, "Cricket Legacy Activity onCreate starting...");
        mHandler = new Handler(Looper.getMainLooper());

        // Initialize native harness
        boolean initOk = nativeCricketHarnessInit();
        Log.i(TAG, "Native Harness Initialization status: " + (initOk ? "SUCCESS" : "FAILED"));

        // Setup SurfaceView
        mSurfaceView = new SurfaceView(this);
        mSurfaceView.getHolder().addCallback(this);
        mSurfaceView.setOnTouchListener(this);
        setContentView(mSurfaceView);

        // Test basic audio output via AudioTrack synthetic sine wave
        testAudioPlayback();
    }

    private void testAudioPlayback() {
        try {
            int sampleRate = 44100;
            int numSamples = sampleRate / 4; // 250ms tone
            short[] buffer = new short[numSamples];
            double freq = 440.0; // 440 Hz 'A' note
            for (int i = 0; i < numSamples; ++i) {
                buffer[i] = (short) (Math.sin(2.0 * Math.PI * i * freq / sampleRate) * 16384);
            }

            AudioTrack track = new AudioTrack(
                AudioManager.STREAM_MUSIC,
                sampleRate,
                AudioFormat.CHANNEL_OUT_MONO,
                AudioFormat.ENCODING_PCM_16BIT,
                numSamples * 2,
                AudioTrack.MODE_STATIC
            );
            track.write(buffer, 0, numSamples);
            track.play();
            Log.i(TAG, "Audio test playback tone successfully triggered!");
            nativeCricketAudioTest();
        } catch (Exception e) {
            Log.e(TAG, "Audio test playback failed: " + e.getMessage());
        }
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        Log.i(TAG, "surfaceCreated: Passing Surface to native Vulkan context...");
        boolean res = nativeCricketSetSurface(holder.getSurface());
        Log.i(TAG, "Native Vulkan surface attachment result: " + res);
        mIsRunning = true;
        mHandler.post(mRenderLoop);
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        Log.i(TAG, "surfaceChanged: Format=" + format + " Dimensions=" + width + "x" + height);
        nativeCricketResize(width, height);
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        Log.i(TAG, "surfaceDestroyed: Detaching native surface...");
        mIsRunning = false;
        mHandler.removeCallbacks(mRenderLoop);
        nativeCricketSurfaceDestroyed();
    }

    @Override
    public boolean onTouch(View v, MotionEvent event) {
        int action = event.getActionMasked();
        int pointerIndex = event.getActionIndex();
        int pointerId = event.getPointerId(pointerIndex);
        float x = event.getX(pointerIndex);
        float y = event.getY(pointerIndex);

        nativeCricketOnTouch(action, x, y, pointerId);
        return true;
    }

    @Override
    protected void onResume() {
        super.onResume();
        Log.i(TAG, "onResume: Resuming render loop...");
        if (mSurfaceView.getHolder().getSurface().isValid()) {
            mIsRunning = true;
            mHandler.post(mRenderLoop);
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        Log.i(TAG, "onPause: Pausing render loop...");
        mIsRunning = false;
        mHandler.removeCallbacks(mRenderLoop);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        Log.i(TAG, "onDestroy: Shutting down native harness...");
        mIsRunning = false;
        mHandler.removeCallbacks(mRenderLoop);
        nativeCricketHarnessShutdown();
    }
}
