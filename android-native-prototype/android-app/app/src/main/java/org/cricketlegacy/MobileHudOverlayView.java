package org.cricketlegacy;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RectF;
import android.view.MotionEvent;
import android.view.View;

import java.util.ArrayList;
import java.util.List;

public class MobileHudOverlayView extends View {

    public interface InputEventListener {
        void onControlModeChanged(int newMode);
        void onTouchForward(int action, float x, float y, int pointerId);
        void onHudLayoutChanged(float[] layout);
    }

    private InputEventListener mListener;

    // Batting modes exposed by G.4: Swipe or Classic Joystick.
    private int mBattingMode = 0;
    private int mGameplayContext = 0;

    // UI Configuration
    private boolean mLeftHanded = false;
    private float mDeadzone = 0.15f;
    private float mSensitivity = 1.0f;
    private boolean mHaptics = true;

    // Drawing Paints (pre-allocated to avoid GC churn during 60 FPS rendering)
    private final Paint mJoystickBasePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint mJoystickKnobPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint mJoystickKnobBorderPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint mJoystickDirectionPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint mButtonBgPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint mButtonPressedPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint mButtonTogglePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint mButtonBorderPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint mButtonTextPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint mSwipeTrailPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint mSwipeLabelPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint mBannerBgPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint mBannerTextPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Path mDirectionMarkerPath = new Path();

    // Joystick State
    private float mJoyBaseX, mJoyBaseY, mJoyRadius;
    private float mJoyKnobX, mJoyKnobY, mJoyKnobRadius;
    private float mViewportLeft, mViewportTop, mViewportWidth, mViewportHeight;
    private float mDensity;
    private boolean mJoyActive = false;
    private int mJoyPointerId = -1;

    // Button Definition
    public static class HudButton {
        int id;
        String label;
        RectF rect = new RectF();
        boolean isPressed = false;
        boolean isToggle = false;
        boolean toggleState = false;
    }

    private final List<HudButton> mButtons = new ArrayList<>();

    // Swipe Visualizer
    private static class SwipeVisual {
        float x0, y0, x1, y1;
        String sectorLabel;
        long timestamp;
        boolean isLoft;
    }
    private final SwipeVisual mActiveSwipe = new SwipeVisual();
    private boolean mHasActiveSwipe = false;

    // Multi-touch tracking
    private static class PointerTrack {
        int id;
        float startX, startY;
        float curX, curY;
        long startTime;
        boolean isJoy;
        HudButton targetButton;
    }
    private final PointerTrack[] mTracks = new PointerTrack[10];

    public MobileHudOverlayView(Context context) {
        super(context);
        init();
    }

    private void init() {
        mDensity = getResources().getDisplayMetrics().density;
        for (int i = 0; i < mTracks.length; i++) {
            mTracks[i] = new PointerTrack();
            mTracks[i].id = -1;
        }

        // Low contrast idle controls preserve the original game's artwork.
        mJoystickBasePaint.setColor(Color.argb(105, 12, 18, 24));
        mJoystickBasePaint.setStyle(Paint.Style.FILL);

        mJoystickKnobPaint.setColor(Color.argb(220, 176, 188, 193));
        mJoystickKnobPaint.setStyle(Paint.Style.FILL);

        mJoystickKnobBorderPaint.setColor(Color.argb(185, 245, 241, 225));
        mJoystickKnobBorderPaint.setStyle(Paint.Style.STROKE);
        mJoystickKnobBorderPaint.setStrokeWidth(1.5f * mDensity);
        mJoystickDirectionPaint.setColor(Color.argb(178, 245, 241, 225));
        mJoystickDirectionPaint.setStyle(Paint.Style.FILL);

        // Buttons
        mButtonBgPaint.setColor(Color.argb(112, 13, 20, 27));
        mButtonBgPaint.setStyle(Paint.Style.FILL);

        mButtonPressedPaint.setColor(Color.argb(218, 190, 145, 66));
        mButtonPressedPaint.setStyle(Paint.Style.FILL);

        mButtonTogglePaint.setColor(Color.argb(214, 151, 111, 50));
        mButtonTogglePaint.setStyle(Paint.Style.FILL);

        mButtonBorderPaint.setColor(Color.argb(132, 238, 239, 228));
        mButtonBorderPaint.setStyle(Paint.Style.STROKE);
        mButtonBorderPaint.setStrokeWidth(1.35f * mDensity);

        mButtonTextPaint.setColor(Color.WHITE);
        mButtonTextPaint.setTextSize(12.0f * mDensity);
        mButtonTextPaint.setTextAlign(Paint.Align.CENTER);
        mButtonTextPaint.setFakeBoldText(true);

        // Swipe trail
        mSwipeTrailPaint.setColor(Color.argb(165, 246, 224, 169));
        mSwipeTrailPaint.setStyle(Paint.Style.STROKE);
        mSwipeTrailPaint.setStrokeWidth(2.0f * mDensity);
        mSwipeTrailPaint.setStrokeCap(Paint.Cap.ROUND);

        mSwipeLabelPaint.setColor(Color.argb(190, 255, 255, 248));
        mSwipeLabelPaint.setTextSize(11.0f * mDensity);
        mSwipeLabelPaint.setTextAlign(Paint.Align.CENTER);
        mSwipeLabelPaint.setFakeBoldText(true);

        // Top banner
        mBannerBgPaint.setColor(Color.argb(160, 10, 15, 25));
        mBannerBgPaint.setStyle(Paint.Style.FILL);

        mBannerTextPaint.setColor(Color.argb(240, 220, 240, 255));
        mBannerTextPaint.setTextSize(24.0f);
        mBannerTextPaint.setTextAlign(Paint.Align.LEFT);
    }

    public void setListener(InputEventListener listener) {
        this.mListener = listener;
    }

    /**
     * Enables gameplay controls only after the native host has positively
     * identified a playable context. Menu and unknown states hide this view so
     * its touch layer cannot intercept the original game's menu input.
     */
    public void setGameplayControlsEnabled(boolean enabled) {
        setVisibility(enabled ? View.VISIBLE : View.GONE);
        if (!enabled) {
            mHasActiveSwipe = false;
            mJoyActive = false;
            mJoyPointerId = -1;
            mJoyKnobX = mJoyBaseX;
            mJoyKnobY = mJoyBaseY;
            for (PointerTrack track : mTracks) {
                track.id = -1;
                track.isJoy = false;
                track.targetButton = null;
            }
            for (HudButton button : mButtons) {
                button.isPressed = false;
                button.toggleState = false;
            }
            invalidate();
        }
    }

    public void setGameplayContext(int context) {
        if (mGameplayContext != context) {
            mHasActiveSwipe = false;
            mJoyActive = false;
            mJoyPointerId = -1;
            mJoyKnobX = mJoyBaseX;
            mJoyKnobY = mJoyBaseY;
            for (HudButton button : mButtons) {
                button.isPressed = false;
                button.toggleState = false;
            }
            for (PointerTrack track : mTracks) {
                track.id = -1;
                track.isJoy = false;
                track.targetButton = null;
            }
        }
        mGameplayContext = context;
        invalidate();
    }

    public void setBattingMode(int mode) {
        mBattingMode = mode == 1 ? 1 : 0;
        mHasActiveSwipe = false;
        invalidate();
    }

    private boolean isButtonVisible(HudButton button) {
        if (mGameplayContext == 1 || mGameplayContext == 2 || mGameplayContext == 5)
            return button.id >= 1 && button.id <= 3;
        if (mGameplayContext == 3) return button.id >= 6 && button.id <= 10;
        if (mGameplayContext == 4) return button.id == 4 || button.id == 5;
        return false;
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        if (mViewportWidth <= 0 || mViewportHeight <= 0) {
            mViewportLeft = 0;
            mViewportTop = 0;
            mViewportWidth = w;
            mViewportHeight = h;
        }
        layoutControls(w, h);
    }

    public void setRenderViewport(int[] bounds) {
        if (bounds == null || bounds.length < 6 || bounds[4] <= 0 || bounds[5] <= 0 ||
                getWidth() <= 0 || getHeight() <= 0) return;
        float sx = (float) getWidth() / bounds[4];
        float sy = (float) getHeight() / bounds[5];
        mViewportLeft = bounds[0] * sx;
        mViewportTop = bounds[1] * sy;
        mViewportWidth = bounds[2] * sx;
        mViewportHeight = bounds[3] * sy;
        layoutControls(getWidth(), getHeight());
    }

    private void layoutControls(int w, int h) {
        if (mViewportWidth <= 0 || mViewportHeight <= 0) {
            mViewportLeft = 0;
            mViewportTop = 0;
            mViewportWidth = w;
            mViewportHeight = h;
        }
        float margin = 24.0f * mDensity;
        float shortSide = Math.min(mViewportWidth, mViewportHeight);
        // The reference control uses a thumb-sized pad. Keep its physical size
        // substantial while clamping it to the rendered viewport on compact screens.
        mJoyRadius = Math.min(84.0f * mDensity, shortSide * 0.16f);
        mJoyKnobRadius = mJoyRadius * 0.34f;
        float joyInset = Math.max(mViewportWidth * 0.21f, mJoyRadius + margin);
        mJoyBaseX = mLeftHanded ? mViewportLeft + mViewportWidth - joyInset : mViewportLeft + joyInset;
        mJoyBaseY = mViewportTop + mViewportHeight * 0.64f;
        mJoyKnobX = mJoyBaseX;
        mJoyKnobY = mJoyBaseY;

        mButtons.clear();
        float btnSz = Math.min(56.0f * mDensity, shortSide * 0.19f);
        btnSz = Math.max(Math.min(48.0f * mDensity, shortSide * 0.22f), btnSz);
        float gap = 12.0f * mDensity;
        float clusterWidth = btnSz * 3 + gap * 2;
        float clusterX = mLeftHanded ? mViewportLeft + margin + mViewportWidth * 0.06f
                : mViewportLeft + mViewportWidth - margin - clusterWidth - mViewportWidth * 0.06f;
        // Keep the two-row action group above the original lower broadcast ticker.
        float upperY = mViewportTop + mViewportHeight * 0.48f;
        float lowerY = upperY + btnSz + gap;

        // Button 1: LOFT (Toggle)
        HudButton loftBtn = new HudButton();
        loftBtn.id = 1;
        loftBtn.label = "SHIFT";
        loftBtn.isToggle = true;
        loftBtn.rect.set(clusterX + btnSz + gap, upperY, clusterX + btnSz * 2 + gap, upperY + btnSz);
        mButtons.add(loftBtn);

        // Button 2: ADVANCE
        HudButton advBtn = new HudButton();
        advBtn.id = 2;
        advBtn.label = "D";
        advBtn.rect.set(clusterX + btnSz * 2 + gap * 2, upperY, clusterX + btnSz * 3 + gap * 2, upperY + btnSz);
        mButtons.add(advBtn);

        // Button 3: DEFEND
        HudButton defBtn = new HudButton();
        defBtn.id = 3;
        defBtn.label = "S";
        defBtn.rect.set(clusterX + btnSz + gap, lowerY, clusterX + btnSz * 2 + gap, lowerY + btnSz);
        mButtons.add(defBtn);

        // Button 4: RUN
        HudButton runBtn = new HudButton();
        runBtn.id = 4;
        runBtn.label = "RUN D";
        float runSize = Math.min(78.0f * mDensity,
                Math.max(48.0f * mDensity, mViewportHeight * 0.12f));
        float runY = mViewportTop + mViewportHeight * 0.64f;
        float runX = mLeftHanded ? mViewportLeft + margin
                : mViewportLeft + mViewportWidth - margin - runSize * 2 - gap;
        runBtn.rect.set(runX, runY, runX + runSize, runY + runSize);
        mButtons.add(runBtn);

        // Button 5: CANCEL RUN
        HudButton cancelBtn = new HudButton();
        cancelBtn.id = 5;
        cancelBtn.label = "CANCEL A";
        cancelBtn.rect.set(runX + runSize + gap, runY, runX + runSize * 2 + gap, runY + runSize);
        mButtons.add(cancelBtn);

        HudButton standardBtn = new HudButton();
        standardBtn.id = 6;
        standardBtn.label = "S";
        standardBtn.rect.set(clusterX, upperY, clusterX + btnSz, upperY + btnSz);
        mButtons.add(standardBtn);

        HudButton swingBtn = new HudButton();
        swingBtn.id = 7;
        swingBtn.label = "A";
        swingBtn.rect.set(clusterX + btnSz + gap, upperY,
                clusterX + btnSz * 2 + gap, upperY + btnSz);
        mButtons.add(swingBtn);

        HudButton cutBtn = new HudButton();
        cutBtn.id = 8;
        cutBtn.label = "D";
        cutBtn.rect.set(clusterX + btnSz * 2 + gap * 2, upperY,
                clusterX + btnSz * 3 + gap * 2, upperY + btnSz);
        mButtons.add(cutBtn);

        HudButton bouncerBtn = new HudButton();
        bouncerBtn.id = 9;
        bouncerBtn.label = "SHIFT";
        bouncerBtn.rect.set(clusterX + btnSz + gap, lowerY,
                clusterX + btnSz * 2 + gap, lowerY + btnSz);
        mButtons.add(bouncerBtn);

        HudButton altBtn = new HudButton();
        altBtn.id = 10;
        altBtn.label = "ALT";
        altBtn.rect.set(clusterX + btnSz * 2 + gap * 2, lowerY,
                clusterX + btnSz * 3 + gap * 2, lowerY + btnSz);
        mButtons.add(altBtn);
        dispatchNativeLayout();
    }

    public void dispatchNativeLayout() {
        if (mListener == null || mViewportWidth <= 0 || mViewportHeight <= 0 || mButtons.size() < 10) return;
        float[] layout = new float[43];
        layout[0] = mJoyBaseX;
        layout[1] = mJoyBaseY;
        layout[2] = mJoyRadius;
        for (int i = 0; i < 10; i++) {
            HudButton button = mButtons.get(i);
            int offset = 3 + i * 4;
            layout[offset] = button.rect.left;
            layout[offset + 1] = button.rect.top;
            layout[offset + 2] = button.rect.width();
            layout[offset + 3] = button.rect.height();
        }
        mListener.onHudLayoutChanged(layout);
    }

    public void cycleControlMode() {
        mBattingMode = (mBattingMode + 1) % 2;
        if (mListener != null) {
            mListener.onControlModeChanged(mBattingMode);
        }
        invalidate();
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        int action = event.getActionMasked();
        int actionIndex = event.getActionIndex();
        long now = System.currentTimeMillis();

        if (action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_POINTER_DOWN) {
            int pId = event.getPointerId(actionIndex);
            float px = event.getX(actionIndex);
            float py = event.getY(actionIndex);

            if (px < mViewportLeft || py < mViewportTop ||
                    px >= mViewportLeft + mViewportWidth || py >= mViewportTop + mViewportHeight) {
                return action == MotionEvent.ACTION_DOWN ? false : true;
            }

            // Find free track slot
            PointerTrack track = getFreeTrack(pId);
            if (track != null) {
                track.id = pId;
                track.startX = track.curX = px;
                track.startY = track.curY = py;
                track.startTime = now;
                track.isJoy = false;
                track.targetButton = null;

                // Check button bounds
                for (HudButton btn : mButtons) {
                    if (!isButtonVisible(btn)) continue;
                    if (btn.rect.contains(px, py)) {
                        track.targetButton = btn;
                        btn.isPressed = true;
                        if (btn.isToggle) {
                            btn.toggleState = !btn.toggleState;
                        }
                        invalidate();
                        break;
                    }
                }

                // Check Joystick zone
                if (track.targetButton == null &&
                    (((mGameplayContext == 1 || mGameplayContext == 2) && mBattingMode == 1) ||
                     mGameplayContext == 3)) {
                    float jdx = px - mJoyBaseX;
                    float jdy = py - mJoyBaseY;
                    float dist = (float) Math.sqrt(jdx * jdx + jdy * jdy);
                    if (dist <= mJoyRadius * 1.5f && !mJoyActive) {
                        track.isJoy = true;
                        mJoyActive = true;
                        mJoyPointerId = pId;
                        updateJoystickKnob(px, py);
                        invalidate();
                    }
                }
            }

            if (mListener != null) {
                mListener.onTouchForward(action, px, py, pId);
            }
            return true;

        } else if (action == MotionEvent.ACTION_MOVE) {
            int pCount = event.getPointerCount();
            for (int i = 0; i < pCount; i++) {
                int pId = event.getPointerId(i);
                float px = event.getX(i);
                float py = event.getY(i);
                PointerTrack track = findTrack(pId);
                if (track != null) {
                    track.curX = px;
                    track.curY = py;
                    if (track.isJoy) {
                        updateJoystickKnob(px, py);
                        invalidate();
                    } else if (mBattingMode == 0 && mGameplayContext == 1 &&
                            track.targetButton == null) {
                        float dx = px - track.startX;
                        float dy = py - track.startY;
                        if (Math.hypot(dx, dy) >= 8.0f * mDensity) {
                            mActiveSwipe.x0 = track.startX;
                            mActiveSwipe.y0 = track.startY;
                            mActiveSwipe.x1 = px;
                            mActiveSwipe.y1 = py;
                            mActiveSwipe.sectorLabel = getSectorLabel(dx, dy);
                            mActiveSwipe.isLoft = mButtons.get(0).toggleState;
                            mActiveSwipe.timestamp = now;
                            mHasActiveSwipe = true;
                            invalidate();
                        }
                    }
                }
                if (mListener != null) {
                    mListener.onTouchForward(action, px, py, pId);
                }
            }
            return true;

        } else if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_POINTER_UP || action == MotionEvent.ACTION_CANCEL) {
            int pId = event.getPointerId(actionIndex);
            float px = event.getX(actionIndex);
            float py = event.getY(actionIndex);

            PointerTrack track = findTrack(pId);
            if (track != null) {
                boolean wasButton = track.targetButton != null;
                boolean wasJoystick = track.isJoy;
                if (track.targetButton != null) {
                    if (!track.targetButton.isToggle) {
                        track.targetButton.isPressed = false;
                    }
                    track.targetButton = null;
                    invalidate();
                }

                if (wasJoystick) {
                    mJoyActive = false;
                    mJoyPointerId = -1;
                    mJoyKnobX = mJoyBaseX;
                    mJoyKnobY = mJoyBaseY;
                    invalidate();
                }
                if (action != MotionEvent.ACTION_CANCEL) mHasActiveSwipe = false;

                track.id = -1;
            }

            if (action == MotionEvent.ACTION_CANCEL) {
                mJoyActive = false;
                mJoyPointerId = -1;
                mJoyKnobX = mJoyBaseX;
                mJoyKnobY = mJoyBaseY;
                mHasActiveSwipe = false;
                for (PointerTrack active : mTracks) {
                    active.id = -1;
                    active.isJoy = false;
                    active.targetButton = null;
                }
                for (HudButton button : mButtons) {
                    button.isPressed = false;
                    button.toggleState = false;
                }
                invalidate();
            }

            if (mListener != null) {
                mListener.onTouchForward(action, px, py, pId);
            }
            return true;
        }

        return super.onTouchEvent(event);
    }

    private void updateJoystickKnob(float px, float py) {
        float jdx = px - mJoyBaseX;
        float jdy = py - mJoyBaseY;
        float dist = (float) Math.sqrt(jdx * jdx + jdy * jdy);
        if (dist > mJoyRadius) {
            mJoyKnobX = mJoyBaseX + (jdx / dist) * mJoyRadius;
            mJoyKnobY = mJoyBaseY + (jdy / dist) * mJoyRadius;
        } else {
            mJoyKnobX = px;
            mJoyKnobY = py;
        }
    }

    private String getSectorLabel(float dx, float dy) {
        double angleRad = Math.atan2(-dy, dx);
        double deg = Math.toDegrees(angleRad);
        if (deg < 0) deg += 360.0;

        if (deg >= 67.5 && deg < 112.5) return "STRAIGHT DRIVE";
        if (deg >= 112.5 && deg < 157.5) return "COVER DRIVE";
        if (deg >= 157.5 && deg < 202.5) return "SQUARE CUT";
        if (deg >= 202.5 && deg < 247.5) return "THIRD MAN / GLANCE";
        if (deg >= 247.5 && deg < 292.5) return "FINE LEG GLANCE";
        if (deg >= 292.5 && deg < 337.5) return "SQUARE LEG FLICK";
        if (deg >= 337.5 || deg < 22.5) return "MID-WICKET PULL";
        return "MID-ON DRIVE";
    }

    private PointerTrack getFreeTrack(int id) {
        for (PointerTrack t : mTracks) {
            if (t.id == -1) return t;
        }
        return null;
    }

    private PointerTrack findTrack(int id) {
        for (PointerTrack t : mTracks) {
            if (t.id == id) return t;
        }
        return null;
    }

    private void drawDirectionMarker(Canvas canvas, float cx, float cy, float size, int direction) {
        float tipX = cx, tipY = cy;
        float baseX = cx, baseY = cy;
        if (direction == 0) { tipY -= size; baseY += size * 0.65f; }
        else if (direction == 1) { tipX += size; baseX -= size * 0.65f; }
        else if (direction == 2) { tipY += size; baseY -= size * 0.65f; }
        else { tipX -= size; baseX += size * 0.65f; }
        float side = size * 0.72f;
        mDirectionMarkerPath.reset();
        mDirectionMarkerPath.moveTo(tipX, tipY);
        if (direction == 0 || direction == 2) {
            mDirectionMarkerPath.lineTo(baseX + side, baseY);
            mDirectionMarkerPath.lineTo(baseX - side, baseY);
        } else {
            mDirectionMarkerPath.lineTo(baseX, baseY + side);
            mDirectionMarkerPath.lineTo(baseX, baseY - side);
        }
        mDirectionMarkerPath.close();
        canvas.drawPath(mDirectionMarkerPath, mJoystickDirectionPaint);
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);

        long now = System.currentTimeMillis();

        // 1. Draw Joystick if Batting Mode is not Pure Swipe
        if (((mGameplayContext == 1 || mGameplayContext == 2 || mGameplayContext == 5) && mBattingMode == 1) ||
                mGameplayContext == 3) {
            canvas.drawCircle(mJoyBaseX, mJoyBaseY, mJoyRadius, mJoystickBasePaint);
            canvas.drawCircle(mJoyBaseX, mJoyBaseY, mJoyRadius, mButtonBorderPaint);
            mButtonBorderPaint.setAlpha(55);
            canvas.drawCircle(mJoyBaseX, mJoyBaseY, mJoyRadius * mDeadzone, mButtonBorderPaint);
            mButtonBorderPaint.setAlpha(132);
            float arrowOffset = mJoyRadius * 0.72f;
            float arrowSize = Math.max(5.0f * mDensity, mJoyRadius * 0.11f);
            drawDirectionMarker(canvas, mJoyBaseX, mJoyBaseY - arrowOffset, arrowSize, 0);
            drawDirectionMarker(canvas, mJoyBaseX + arrowOffset, mJoyBaseY, arrowSize, 1);
            drawDirectionMarker(canvas, mJoyBaseX, mJoyBaseY + arrowOffset, arrowSize, 2);
            drawDirectionMarker(canvas, mJoyBaseX - arrowOffset, mJoyBaseY, arrowSize, 3);
            canvas.drawCircle(mJoyKnobX, mJoyKnobY, mJoyKnobRadius, mJoystickKnobPaint);
            canvas.drawCircle(mJoyKnobX, mJoyKnobY, mJoyKnobRadius, mJoystickKnobBorderPaint);
        }

        // 2. Draw Virtual Action Buttons
        for (HudButton btn : mButtons) {
            if (!isButtonVisible(btn)) continue;
            Paint bg = btn.isPressed ? mButtonPressedPaint :
                       (btn.isToggle && btn.toggleState ? mButtonTogglePaint : mButtonBgPaint);
            float buttonRadius = Math.min(btn.rect.width(), btn.rect.height()) * 0.5f;
            canvas.drawCircle(btn.rect.centerX(), btn.rect.centerY(), buttonRadius, bg);
            canvas.drawCircle(btn.rect.centerX(), btn.rect.centerY(), buttonRadius, mButtonBorderPaint);

            float labelSizeDp = btn.label.length() >= 7 ? 10.0f :
                    (btn.label.length() >= 6 ? 10.8f : 14.0f);
            mButtonTextPaint.setTextSize(labelSizeDp * mDensity);
            float textY = btn.rect.centerY() - ((mButtonTextPaint.descent() + mButtonTextPaint.ascent()) / 2);
            canvas.drawText(btn.label, btn.rect.centerX(), textY, mButtonTextPaint);
        }
        mButtonTextPaint.setTextSize(12.0f * mDensity);

        // Direction and sector feedback exists only while the finger is down.
        if (mHasActiveSwipe && mBattingMode == 0 && mGameplayContext == 1) {
            mSwipeTrailPaint.setAlpha(170);
            mSwipeLabelPaint.setAlpha(220);
            canvas.drawLine(mActiveSwipe.x0, mActiveSwipe.y0,
                    mActiveSwipe.x1, mActiveSwipe.y1, mSwipeTrailPaint);
            canvas.drawCircle(mActiveSwipe.x1, mActiveSwipe.y1,
                    4.0f * mDensity, mSwipeTrailPaint);
            String label = mActiveSwipe.sectorLabel + (mActiveSwipe.isLoft ? " · LOFT" : "");
            canvas.drawText(label, mActiveSwipe.x1,
                    mActiveSwipe.y1 - 12.0f * mDensity, mSwipeLabelPaint);
            postInvalidateOnAnimation();
        }
    }
}
