package com.rockcarry.fanplayer;

import android.content.Context;
import android.widget.RelativeLayout;
import android.util.AttributeSet;

public class playerView extends RelativeLayout {
    private OnSizeChangedListener mListener = null;

    public playerView(Context context) {
        super(context);
    }

    public playerView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        if (mListener != null) {
            mListener.onSizeChanged(w, h, oldw, oldh);
        }
    }

    public interface OnSizeChangedListener {
        public void onSizeChanged(int w, int h, int oldw, int oldh);
    }

    public void setOnSizeChangedListener(OnSizeChangedListener l) {
        mListener = l;
    }
}

