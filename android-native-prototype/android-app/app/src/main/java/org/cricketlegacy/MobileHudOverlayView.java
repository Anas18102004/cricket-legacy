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
    }

    private InputEventListener mListener;

    // Batting Modes: 0=Pure Swipe, 1=Classic Joystick, 2=Pro Joystick, 3=Hybrid
    private int mBattingMode = 3;
    private final String[] MODE_NAMES = {
        "SWIPE ONLY",
        "CLASSIC STICK",
        "PRO JOYSTICK",
        "HYBRID CONTROLS"
    };

    // UI Configuration
    private boolean mLeftHanded = false;
    private float mDeadzone = 0.15f;
    private float mSensitivity = 1.0f;
    private boolean mHaptics = true;

    // Drawing Paints (pre-allocated to avoid GC churn during 60 FPS rendering)
    private final Paint mJoystickBasePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint mJoystickKnobPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint mButtonBgPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint mButtonPressedPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint mButtonTogglePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint mButtonBorderPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint mButtonTextPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint mSwipeTrailPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint mSwipeLabelPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint mBannerBgPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint mBannerTextPaint = new Paint(Paint.ANTI_ALIAS_FLAG);

    // Joystick State
    private float mJoyBaseX, mJoyBaseY, mJoyRadius;
    private float mJoyKnobX, mJoyKnobY, mJoyKnobRadius;
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
    private final RectF mModePillRect = new RectF();

    // Swipe Visualizer
    private static class SwipeVisual {
        float x0, y0, x1, y1;
        String sectorLabel;
        long timestamp;
        boolean isLoft;
    }
    private final List<SwipeVisual> mRecentSwipes = new ArrayList<>();

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
        for (int i = 0; i < mTracks.length; i++) {
            mTracks[i] = new PointerTrack();
            mTracks[i].id = -1;
        }

        // Joystick base: Translucent dark slate with cyan rim
        mJoystickBasePaint.setColor(Color.argb(80, 20, 30, 45));
        mJoystickBasePaint.setStyle(Paint.Style.FILL);

        // Joystick knob: Glowing cyan
        mJoystickKnobPaint.setColor(Color.argb(200, 0, 220, 255));
        mJoystickKnobPaint.setStyle(Paint.Style.FILL);

        // Buttons
        mButtonBgPaint.setColor(Color.argb(120, 25, 35, 55));
        mButtonBgPaint.setStyle(Paint.Style.FILL);

        mButtonPressedPaint.setColor(Color.argb(220, 0, 180, 240));
        mButtonPressedPaint.setStyle(Paint.Style.FILL);

        mButtonTogglePaint.setColor(Color.argb(220, 255, 175, 0)); // Golden Amber
        mButtonTogglePaint.setStyle(Paint.Style.FILL);

        mButtonBorderPaint.setColor(Color.argb(180, 100, 180, 255));
        mButtonBorderPaint.setStyle(Paint.Style.STROKE);
        mButtonBorderPaint.setStrokeWidth(3.0f);

        mButtonTextPaint.setColor(Color.WHITE);
        mButtonTextPaint.setTextSize(26.0f);
        mButtonTextPaint.setTextAlign(Paint.Align.CENTER);
        mButtonTextPaint.setFakeBoldText(true);

        // Swipe trail
        mSwipeTrailPaint.setColor(Color.argb(230, 0, 240, 255));
        mSwipeTrailPaint.setStyle(Paint.Style.STROKE);
        mSwipeTrailPaint.setStrokeWidth(6.0f);
        mSwipeTrailPaint.setStrokeCap(Paint.Cap.ROUND);

        mSwipeLabelPaint.setColor(Color.argb(250, 255, 255, 255));
        mSwipeLabelPaint.setTextSize(32.0f);
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

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        layoutControls(w, h);
    }

    private void layoutControls(int w, int h) {
        float scale = Math.min((float) w / 1280.0f, (float) h / 720.0f);
        if (scale < 0.7f) scale = 0.7f;
        if (scale > 1.4f) scale = 1.4f;

        // Joystick dimensions
        mJoyRadius = 110.0f * scale;
        mJoyKnobRadius = 45.0f * scale;
        if (!mLeftHanded) {
            mJoyBaseX = 180.0f * scale;
            mJoyBaseY = h - 180.0f * scale;
        } else {
            mJoyBaseX = w - 180.0f * scale;
            mJoyBaseY = h - 180.0f * scale;
        }
        mJoyKnobX = mJoyBaseX;
        mJoyKnobY = mJoyBaseY;

        // Mode Pill Top Right
        float pillW = 220.0f * scale;
        float pillH = 44.0f * scale;
        mModePillRect.set(w - pillW - 24.0f, 20.0f, w - 24.0f, 20.0f + pillH);

        // Action Buttons Setup
        mButtons.clear();
        float btnSz = 90.0f * scale;
        float gap = 24.0f * scale;
        float rightClusterX = !mLeftHanded ? (w - 240.0f * scale) : (60.0f * scale);
        float rightClusterY = h - 220.0f * scale;

        // Button 1: LOFT (Toggle)
        HudButton loftBtn = new HudButton();
        loftBtn.id = 1;
        loftBtn.label = "LOFT";
        loftBtn.isToggle = true;
        loftBtn.rect.set(rightClusterX, rightClusterY - btnSz - gap, rightClusterX + btnSz, rightClusterY - gap);
        mButtons.add(loftBtn);

        // Button 2: ADVANCE
        HudButton advBtn = new HudButton();
        advBtn.id = 2;
        advBtn.label = "ADVANCE";
        advBtn.rect.set(rightClusterX + btnSz + gap, rightClusterY - btnSz - gap, rightClusterX + btnSz * 2 + gap, rightClusterY - gap);
        mButtons.add(advBtn);

        // Button 3: DEFEND
        HudButton defBtn = new HudButton();
        defBtn.id = 3;
        defBtn.label = "DEFEND";
        defBtn.rect.set(rightClusterX - btnSz - gap, rightClusterY, rightClusterX - gap, rightClusterY + btnSz);
        mButtons.add(defBtn);

        // Button 4: RUN
        HudButton runBtn = new HudButton();
        runBtn.id = 4;
        runBtn.label = "RUN";
        runBtn.rect.set(rightClusterX, rightClusterY, rightClusterX + btnSz, rightClusterY + btnSz);
        mButtons.add(runBtn);

        // Button 5: CANCEL RUN
        HudButton cancelBtn = new HudButton();
        cancelBtn.id = 5;
        cancelBtn.label = "CANCEL";
        cancelBtn.rect.set(rightClusterX + btnSz + gap, rightClusterY, rightClusterX + btnSz * 2 + gap, rightClusterY + btnSz);
        mButtons.add(cancelBtn);
    }

    public void cycleControlMode() {
        mBattingMode = (mBattingMode + 1) % 4;
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

            // 1. Check Mode Switch Pill
            if (mModePillRect.contains(px, py)) {
                cycleControlMode();
                return true;
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
                if (track.targetButton == null) {
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
                if (track.targetButton != null) {
                    if (!track.targetButton.isToggle) {
                        track.targetButton.isPressed = false;
                    }
                    track.targetButton = null;
                    invalidate();
                }

                if (track.isJoy) {
                    mJoyActive = false;
                    mJoyPointerId = -1;
                    mJoyKnobX = mJoyBaseX;
                    mJoyKnobY = mJoyBaseY;
                    invalidate();
                } else if (track.targetButton == null) {
                    // Evaluate visual swipe
                    float dx = px - track.startX;
                    float dy = py - track.startY;
                    float dist = (float) Math.sqrt(dx * dx + dy * dy);
                    long duration = now - track.startTime;
                    if (dist > 30.0f && duration < 500) {
                        SwipeVisual sv = new SwipeVisual();
                        sv.x0 = track.startX;
                        sv.y0 = track.startY;
                        sv.x1 = px;
                        sv.y1 = py;
                        sv.timestamp = now;
                        sv.isLoft = mButtons.get(0).toggleState;
                        sv.sectorLabel = getSectorLabel(dx, dy);
                        synchronized (mRecentSwipes) {
                            mRecentSwipes.add(sv);
                            if (mRecentSwipes.size() > 5) mRecentSwipes.remove(0);
                        }
                        invalidate();
                    }
                }

                track.id = -1;
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

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);

        long now = System.currentTimeMillis();

        // 1. Draw Joystick if Batting Mode is not Pure Swipe
        if (mBattingMode != 0) {
            // Base ring
            canvas.drawCircle(mJoyBaseX, mJoyBaseY, mJoyRadius, mJoystickBasePaint);
            canvas.drawCircle(mJoyBaseX, mJoyBaseY, mJoyRadius, mButtonBorderPaint);
            // Deadzone guide
            canvas.drawCircle(mJoyBaseX, mJoyBaseY, mJoyRadius * mDeadzone, mButtonBorderPaint);
            // Thumb knob
            canvas.drawCircle(mJoyKnobX, mJoyKnobY, mJoyKnobRadius, mJoystickKnobPaint);
        }

        // 2. Draw Virtual Action Buttons
        for (HudButton btn : mButtons) {
            Paint bg = btn.isPressed ? mButtonPressedPaint :
                       (btn.isToggle && btn.toggleState ? mButtonTogglePaint : mButtonBgPaint);
            canvas.drawRoundRect(btn.rect, 16.0f, 16.0f, bg);
            canvas.drawRoundRect(btn.rect, 16.0f, 16.0f, mButtonBorderPaint);

            float textY = btn.rect.centerY() - ((mButtonTextPaint.descent() + mButtonTextPaint.ascent()) / 2);
            canvas.drawText(btn.label, btn.rect.centerX(), textY, mButtonTextPaint);
        }

        // 3. Draw Top Control Mode Selector Pill
        canvas.drawRoundRect(mModePillRect, 22.0f, 22.0f, mButtonBgPaint);
        canvas.drawRoundRect(mModePillRect, 22.0f, 22.0f, mButtonBorderPaint);
        float pillTextY = mModePillRect.centerY() - ((mButtonTextPaint.descent() + mButtonTextPaint.ascent()) / 2);
        canvas.drawText(MODE_NAMES[mBattingMode], mModePillRect.centerX(), pillTextY, mButtonTextPaint);

        // 4. Draw Recent Swipe Visualizer Trails
        boolean hasActiveSwipe = false;
        synchronized (mRecentSwipes) {
            for (int i = mRecentSwipes.size() - 1; i >= 0; i--) {
                SwipeVisual sv = mRecentSwipes.get(i);
                long elapsed = now - sv.timestamp;
                if (elapsed > 400) {
                    mRecentSwipes.remove(i);
                } else {
                    hasActiveSwipe = true;
                    int alpha = (int) (240.0f * (1.0f - (float) elapsed / 400.0f));
                    mSwipeTrailPaint.setAlpha(alpha);
                    mSwipeLabelPaint.setAlpha(alpha);

                    // Draw glowing stroke line
                    canvas.drawLine(sv.x0, sv.y0, sv.x1, sv.y1, mSwipeTrailPaint);
                    canvas.drawCircle(sv.x1, sv.y1, 10.0f, mSwipeTrailPaint);

                    // Draw shot text near stroke tip
                    String label = sv.isLoft ? "LOFTED " + sv.sectorLabel : sv.sectorLabel;
                    canvas.drawText(label, (sv.x0 + sv.x1) * 0.5f, Math.min(sv.y0, sv.y1) - 20.0f, mSwipeLabelPaint);
                }
            }
        }

        // Keep animating swipe fadeout
        if (hasActiveSwipe) {
            postInvalidateOnAnimation();
        }
    }
}
