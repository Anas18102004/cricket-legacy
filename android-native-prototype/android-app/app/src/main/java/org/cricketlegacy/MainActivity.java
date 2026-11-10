package org.cricketlegacy;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.content.SharedPreferences;
import android.util.Log;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.WindowManager;
import android.widget.FrameLayout;

public class MainActivity extends Activity implements SurfaceHolder.Callback, View.OnTouchListener, MobileHudOverlayView.InputEventListener {
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
    public static native void nativeCricketOnHudTouch(int action, float x, float y, int pointerId);
    public static native void nativeCricketRenderFrame();
    public static native boolean nativeCricketAudioTest();
    public static native void nativeCricketHarnessShutdown();

    // Mobile Input JNI Declarations
    public static native void nativeCricketConfigureInput(int battingMode, int bowlingMode,
                                                          float deadzone, float sensitivity,
                                                          boolean leftHanded, boolean haptics);
    public static native boolean nativeCricketPollHaptic();
    public static native long nativeCricketGetInputLatencyUs();
    public static native void nativeCricketSetGamePhase(int phase);
    public static native int nativeCricketGetRenderContext();
    public static native int[] nativeCricketGetRenderViewport();
    public static native void nativeCricketSetHudLayout(float[] layout);
    public static native void nativeCricketInvalidateRenderContext();

    private SurfaceView mSurfaceView;
    private MobileHudOverlayView mHudOverlay;
    private Vibrator mVibrator;
    private Handler mHandler;
    private boolean mIsRunning = false;
    private static final int HUD_OFF = 0;
    private static final int HUD_BATTING_SWIPE = 1;
    private static final int HUD_BATTING_CLASSIC = 2;
    private static final int HUD_BOWLING = 3;
    private static final int HUD_RUNNING = 4;
    private static final int HUD_BALL_DEAD = 5;
    private static final int HUD_FIELDING = 6;
    private static final int HUD_REPLAY = 7;
    private int mSelectedHudContext = HUD_OFF;
    private int mBattingMode = 0;
    private boolean mHudVisible = false;
    private int mAppliedRenderContext = Integer.MIN_VALUE;
    private int mAppliedHudContext = Integer.MIN_VALUE;
    private final String[] mHudContextChoices = {
        "Hide controls",
        "Batting - Swipe",
        "Batting - Classic Joystick",
        "Bowling controls",
        "Running - RUN / CANCEL",
        "Ball dead - batting controls only",
        "Fielding - automatic, no controls",
        "Replay - hide controls"
    };

    private final Runnable mRenderLoop = new Runnable() {
        @Override
        public void run() {
            if (mIsRunning) {
                nativeCricketRenderFrame();
                refreshHudVisibility();

                // Poll native haptic pulse
                if (nativeCricketPollHaptic() && mVibrator != null && mVibrator.hasVibrator()) {
                    triggerHapticFeedback();
                }

                mHandler.postDelayed(this, 16); // ~60 FPS update pacing
            }
        }
    };

    private final android.content.BroadcastReceiver mContextReceiver = new android.content.BroadcastReceiver() {
        @Override
        public void onReceive(Context context, android.content.Intent intent) {
            int ctx = intent.getIntExtra("context", -1);
            if (ctx >= 0) {
                Log.i(TAG, "Broadcast received: setting HUD context to " + ctx);
                selectHudContext(ctx);
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
        mVibrator = (Vibrator) getSystemService(Context.VIBRATOR_SERVICE);
        SharedPreferences controlPrefs = getSharedPreferences("mobile_controls", MODE_PRIVATE);
        mBattingMode = controlPrefs.getInt("batting_mode", 0) == 1 ? 1 : 0;

        // Initialize native harness
        boolean initOk = nativeCricketHarnessInit();
        Log.i(TAG, "Native Harness Initialization status: " + (initOk ? "SUCCESS" : "FAILED"));

        // Setup Root Layout containing SurfaceView + Mobile HUD Overlay
        FrameLayout rootLayout = new FrameLayout(this);

        mSurfaceView = new SurfaceView(this);
        mSurfaceView.getHolder().addCallback(this);
        // With the gameplay HUD fail-closed, the surface must continue receiving
        // touch input so the original Cricket 07 menus remain navigable.
        mSurfaceView.setOnTouchListener(this);
        rootLayout.addView(mSurfaceView, new FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT));

        mHudOverlay = new MobileHudOverlayView(this);
        mHudOverlay.setListener(this);
        // The host currently exposes no verified menu/match/ball phase signal.
        // Never guess from frame counts: hide gameplay controls until one exists.
        mHudOverlay.setGameplayControlsEnabled(false);
        mHudOverlay.setBattingMode(mBattingMode);
        rootLayout.addView(mHudOverlay, new FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT));

        setContentView(rootLayout);

        // Active gameplay context always starts hidden, even when a batting mode
        // was persisted. The user explicitly selects a context for each session.
        nativeCricketConfigureInput(mBattingMode, 0 /* gesture aim */, 0.15f, 1.0f, false, true);
        nativeCricketSetGamePhase(0 /* hidden */);

        // Test basic audio output via AudioTrack synthetic sine wave
        testAudioPlayback();

        // Register dynamic HUD context receiver for external testing/diagnostics
        registerReceiver(mContextReceiver, new android.content.IntentFilter("org.cricketlegacy.SET_CONTEXT"), Context.RECEIVER_EXPORTED);
    }

    private void triggerHapticFeedback() {
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                mVibrator.vibrate(VibrationEffect.createOneShot(25, VibrationEffect.DEFAULT_AMPLITUDE));
            } else {
                mVibrator.vibrate(25);
            }
        } catch (Exception e) {
            Log.w(TAG, "Haptic vibration failed: " + e.getMessage());
        }
    }

    @Override
    public void onControlModeChanged(int newMode) {
        setBattingMode(newMode);
    }

    @Override
    public void onTouchForward(int action, float x, float y, int pointerId) {
        nativeCricketOnHudTouch(action, x, y, pointerId);
    }

    @Override
    public void onHudLayoutChanged(float[] layout) {
        nativeCricketSetHudLayout(layout);
    }

    private void setBattingMode(int mode) {
        mBattingMode = mode == 1 ? 1 : 0;
        getSharedPreferences("mobile_controls", MODE_PRIVATE).edit()
            .putInt("batting_mode", mBattingMode).apply();
        if (mHudOverlay != null) mHudOverlay.setBattingMode(mBattingMode);
        nativeCricketConfigureInput(mBattingMode, 0, 0.15f, 1.0f, false, true);
        if (mHudOverlay != null) mHudOverlay.dispatchNativeLayout();
    }

    private int nativePhaseForHudContext(int context) {
        switch (context) {
            case HUD_BATTING_SWIPE:
            case HUD_BATTING_CLASSIC: return 1; // user-selected batting context
            case HUD_BOWLING: return 2;
            case HUD_FIELDING: return 3;
            case HUD_RUNNING: return 4;
            case HUD_BALL_DEAD: return 5;
            case HUD_REPLAY: return 6;
            default: return 0;
        }
    }

    private void selectHudContext(int context) {
        mSelectedHudContext = context;
        mAppliedHudContext = Integer.MIN_VALUE;
        if (context == HUD_BATTING_SWIPE) setBattingMode(0);
        else if (context == HUD_BATTING_CLASSIC) setBattingMode(1);
        if (mHudOverlay != null) mHudOverlay.setGameplayContext(context);
        refreshHudVisibility();
    }

    private void refreshHudVisibility() {
        if (mHudOverlay == null) return;
        int renderContext = nativeCricketGetRenderContext();
        if (renderContext == 0 && mSelectedHudContext == HUD_OFF) {
            // Auto-arm player's saved batting preference when entering open match play
            mSelectedHudContext = (mBattingMode == 1) ? HUD_BATTING_CLASSIC : HUD_BATTING_SWIPE;
        }
        boolean requested = mSelectedHudContext >= HUD_BATTING_SWIPE &&
            mSelectedHudContext <= HUD_BALL_DEAD;
        boolean shouldShow = requested && renderContext == 0;
        if (renderContext != mAppliedRenderContext || mSelectedHudContext != mAppliedHudContext) {
            if (shouldShow) nativeCricketSetGamePhase(nativePhaseForHudContext(mSelectedHudContext));
            else nativeCricketSetGamePhase(0); // also clears any held touch input
            mHudOverlay.setGameplayContext(mSelectedHudContext);
            mHudOverlay.setGameplayControlsEnabled(shouldShow);
            mHudVisible = shouldShow;
            mAppliedRenderContext = renderContext;
            mAppliedHudContext = mSelectedHudContext;
            Log.i(TAG, "HUD state selected=" + mSelectedHudContext + " render=" + renderContext +
                    " visible=" + shouldShow);
        } else if (shouldShow != mHudVisible) {
            if (shouldShow) nativeCricketSetGamePhase(nativePhaseForHudContext(mSelectedHudContext));
            else nativeCricketSetGamePhase(0);
            mHudOverlay.setGameplayControlsEnabled(shouldShow);
            mHudVisible = shouldShow;
        }
    }

    @Override
    public void onBackPressed() {
        new AlertDialog.Builder(this)
            .setTitle("Mobile controls - choose current context")
            .setSingleChoiceItems(mHudContextChoices, mSelectedHudContext, (dialog, which) -> {
                selectHudContext(which);
                dialog.dismiss();
            })
            .setNegativeButton("Close", (dialog, which) -> dialog.dismiss())
            .show();
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
        if (mHudOverlay != null) mHudOverlay.setRenderViewport(nativeCricketGetRenderViewport());
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        Log.i(TAG, "surfaceDestroyed: Detaching native surface...");
        mIsRunning = false;
        mHandler.removeCallbacks(mRenderLoop);
        nativeCricketInvalidateRenderContext();
        mSelectedHudContext = HUD_OFF;
        mHudVisible = false;
        mAppliedRenderContext = Integer.MIN_VALUE;
        mAppliedHudContext = Integer.MIN_VALUE;
        nativeCricketSetGamePhase(0);
        if (mHudOverlay != null) mHudOverlay.setGameplayControlsEnabled(false);
        nativeCricketSurfaceDestroyed();
    }

    @Override
    public boolean onTouch(View v, MotionEvent event) {
        int action = event.getActionMasked();
        if (action == MotionEvent.ACTION_MOVE) {
            int pointerCount = event.getPointerCount();
            for (int i = 0; i < pointerCount; i++) {
                nativeCricketOnTouch(action, event.getX(i), event.getY(i), event.getPointerId(i));
            }
        } else {
            int pointerIndex = event.getActionIndex();
            int pointerId = event.getPointerId(pointerIndex);
            float x = event.getX(pointerIndex);
            float y = event.getY(pointerIndex);
            nativeCricketOnTouch(action, x, y, pointerId);
        }
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
        nativeCricketInvalidateRenderContext();
        mSelectedHudContext = HUD_OFF;
        mHudVisible = false;
        mAppliedRenderContext = Integer.MIN_VALUE;
        mAppliedHudContext = Integer.MIN_VALUE;
        if (mHudOverlay != null) mHudOverlay.setGameplayControlsEnabled(false);
        nativeCricketSetGamePhase(0);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        Log.i(TAG, "onDestroy: Shutting down native harness...");
        try {
            unregisterReceiver(mContextReceiver);
        } catch (Exception ignored) {}
        mIsRunning = false;
        mHandler.removeCallbacks(mRenderLoop);
        nativeCricketInvalidateRenderContext();
        nativeCricketHarnessShutdown();
    }
}
