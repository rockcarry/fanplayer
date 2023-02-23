package com.rockcarry.fanplayer;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.provider.MediaStore;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceHolder.Callback;
import android.view.SurfaceView;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.ProgressBar;
import android.widget.RelativeLayout;
import android.widget.SeekBar;
import android.widget.Toast;
import android.util.Log;

import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;

public class MainActivity extends Activity {
    private static final String TAG = "fanplayer";
    private static final String PLAYER_INIT_PARAMS = "video_hwaccel=1;init_timeout=2000;auto_reconnect=2000;audio_bufpktn=4;video_bufpktn=1;rtsp_transport=2;";
    private static final String PLAYER_SHARED_PREFS= "fanplayer_shared_prefs";
    private static final String KEY_PLAYER_OPEN_URL= "key_player_open_url";
    private static final String DEF_PLAYER_OPEN_URL= "rtsp://192.168.0.148/video0";
    private MediaPlayer  mPlayer    = null;
    private playerView   mRoot      = null;
    private SurfaceView  mVideo     = null;
    private SeekBar      mSeek      = null;
    private ProgressBar  mBuffering = null;
    private ImageView    mPause     = null;
    private boolean      mIsPlaying = false;
    private boolean      mIsLive    = false;
    private String       mURL       = "";
    private Surface      mVideoSurface;
    private int          mVideoViewW;
    private int          mVideoViewH;
    SharedPreferences    mSharedPrefs;

    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        try {
            PackageInfo packageInfo = getPackageManager().getPackageInfo(getApplicationInfo().packageName, PackageManager.GET_PERMISSIONS);
            if (packageInfo.requestedPermissions != null) {
                for (String permission : packageInfo.requestedPermissions) {
                    Log.v(TAG, "Checking permissions for: " + permission);
                    if (checkSelfPermission(permission) != PackageManager.PERMISSION_GRANTED) {
                        requestPermissions(packageInfo.requestedPermissions, 1);
                    }
                }
            }
        } catch (NameNotFoundException e) {
            Log.e(TAG, "Unable to load package's permissions", e);
        }

        setContentView(R.layout.main);
        mSharedPrefs = getSharedPreferences(PLAYER_SHARED_PREFS, Context.MODE_PRIVATE);

        Intent intent = getIntent();
        String action = intent.getAction();
        if (intent.ACTION_VIEW.equals(action)) {
            Uri    uri    = (Uri) intent.getData();
            String scheme = uri.getScheme();
            if (scheme.equals("file")) {
                mURL = uri.getPath();
            } else if (  scheme.equals("http" )
                      || scheme.equals("https")
                      || scheme.equals("rtsp" )
                      || scheme.equals("rtmp" ) ) {
                mURL = uri.toString();
            } else if (scheme.equals("content")) {
                String[] proj = { MediaStore.Images.Media.DATA };
                Cursor cursor = managedQuery(uri, proj, null, null, null);
                int    colidx = cursor.getColumnIndexOrThrow(MediaStore.Images.Media.DATA);
                cursor.moveToFirst();
                mURL = cursor.getString(colidx);
            }
            mIsLive = mURL.startsWith("http://") && mURL.endsWith(".m3u8") || mURL.startsWith("rtmp://") || mURL.startsWith("rtsp://") || mURL.startsWith("avkcp://") || mURL.startsWith("ffrdp://");
            mPlayer = new MediaPlayer(mURL, mHandler, PLAYER_INIT_PARAMS);
        } else {
            mURL = readPlayerOpenURL();
            AlertDialog.Builder builder = new AlertDialog.Builder(this);
            builder.setTitle("open url:");
            final EditText edt = new EditText(this);
            edt.setHint("url:");
            edt.setSingleLine(true);
            edt.setText(mURL);
            builder.setView(edt);
            builder.setNegativeButton("cancel" , new DialogInterface.OnClickListener() {
                @Override
                public void onClick(DialogInterface dialog, int which) {
                    MainActivity.this.finish();
                }
            });
            builder.setPositiveButton("confirm", new DialogInterface.OnClickListener() {
                @Override
                public void onClick(DialogInterface dialog, int which) {
                    mURL = edt.getText().toString();
                    savePlayerOpenURL(mURL);
                    mIsLive = mURL.startsWith("http://") && mURL.endsWith(".m3u8") || mURL.startsWith("rtmp://") || mURL.startsWith("rtsp://") || mURL.startsWith("avkcp://") || mURL.startsWith("ffrdp://");
                    mPlayer = new MediaPlayer(mURL, mHandler, PLAYER_INIT_PARAMS);
                    mPlayer.setDisplaySurface(mVideoSurface);
                    testPlayerPlay(true);
                }
            });
            AlertDialog dlg = builder.create();
            dlg.show();
        }

        mRoot = (playerView)findViewById(R.id.player_root);
        mRoot.setOnSizeChangedListener(new playerView.OnSizeChangedListener() {
            @Override
            public void onSizeChanged(int w, int h, int oldw, int oldh) {
                mVideo.setVisibility(View.INVISIBLE);
                mVideoViewW = w;
                mVideoViewH = h;
                mHandler.sendEmptyMessage(MSG_UDPATE_VIEW_SIZE);
            }
        });

        mVideo = (SurfaceView)findViewById(R.id.video_view);
        mVideo.getHolder().addCallback(
            new Callback() {
                @Override
                public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
//                  if (mPlayer != null) mPlayer.setDisplaySurface(holder.getSurface());
                }

                @Override
                public void surfaceCreated(SurfaceHolder holder) {
                    mVideoSurface = holder.getSurface();
                    if (mPlayer != null) mPlayer.setDisplaySurface(mVideoSurface);
                }

                @Override
                public void surfaceDestroyed(SurfaceHolder holder) {
                    mVideoSurface = null;
                    if (mPlayer != null) mPlayer.setDisplaySurface(mVideoSurface);
                }
            }
        );

        mSeek = (SeekBar)findViewById(R.id.seek_bar);
        mSeek.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                if (fromUser) {
                    if (mPlayer != null) mPlayer.seek(progress);
                }
            }
            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {
            }
            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
            }
        });

        mPause = (ImageView)findViewById(R.id.btn_playpause);
        mPause.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                testPlayerPlay(!mIsPlaying);
            }
        });

        mBuffering = (ProgressBar)findViewById(R.id.buffering);
        mBuffering.setVisibility(mIsLive ? View.VISIBLE : View.INVISIBLE);

        // show buttons with auto hide
        showUIControls(true, true);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        mHandler.removeMessages(MSG_UPDATE_PROGRESS);
        if (mPlayer != null) mPlayer.close();
    }

    @Override
    public void onResume() {
        super.onResume();
        if (!mIsLive) testPlayerPlay(true);
    }

    @Override
    public void onPause() {
        if (!mIsLive) testPlayerPlay(false);
        super.onPause();
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent ev) {
        if (ev.getActionMasked() == MotionEvent.ACTION_DOWN) {
            showUIControls(true, true);
        }
        return super.dispatchTouchEvent(ev);
    }

    private void testPlayerPlay(boolean play) {
        if (mPlayer == null) return;
        if (play) {
            mPlayer.play();
            mIsPlaying = true;
            mPause  .setImageResource(R.drawable.icn_media_pause);
            mHandler.sendEmptyMessage(MSG_UPDATE_PROGRESS);
        } else {
            mPlayer.pause();
            mIsPlaying = false;
            mPause  .setImageResource(R.drawable.icn_media_play );
            mHandler.removeMessages  (MSG_UPDATE_PROGRESS);
        }
    }

    private void showUIControls(boolean show, boolean autohide) {
        mHandler.removeMessages(MSG_HIDE_BUTTONS);
        if (mIsLive) show = false;
        if (show) {
            mSeek .setVisibility(View.VISIBLE);
            mPause.setVisibility(View.VISIBLE);
            if (autohide) {
                mHandler.sendEmptyMessageDelayed(MSG_HIDE_BUTTONS, 5000);
            }
        }
        else {
            mSeek .setVisibility(View.INVISIBLE);
            mPause.setVisibility(View.INVISIBLE);
        }
    }

    private static final int MSG_UPDATE_PROGRESS  = 1;
    private static final int MSG_UDPATE_VIEW_SIZE = 2;
    private static final int MSG_HIDE_BUTTONS     = 3;
    private Handler mHandler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            switch (msg.what) {
            case MSG_UPDATE_PROGRESS: {
                    mHandler.sendEmptyMessageDelayed(MSG_UPDATE_PROGRESS, 200);
                    int progress = mPlayer != null ? (int)mPlayer.getParam(MediaPlayer.PARAM_MEDIA_POSITION) : 0;
                    if (!mIsLive) {
                        if (progress >= 0) mSeek.setProgress(progress);
                    } else {
                        mBuffering.setVisibility(progress == -1 ? View.VISIBLE : View.INVISIBLE);
                    }
                }
                break;
            case MSG_HIDE_BUTTONS: {
                    mSeek .setVisibility(View.INVISIBLE);
                    mPause.setVisibility(View.INVISIBLE);
                }
                break;
            case MSG_UDPATE_VIEW_SIZE: {
                    if (mPlayer != null && mPlayer.initVideoSize(mVideoViewW, mVideoViewH, mVideo)) {
                        mVideo.setVisibility(View.VISIBLE);
                    }
                }
                break;
            case MediaPlayer.MSG_OPEN_DONE: {
                    if (mPlayer != null) {
                        mPlayer .setDisplaySurface(mVideoSurface);
                        mVideo  .setVisibility(View.INVISIBLE);
                        mHandler.sendEmptyMessage(MSG_UDPATE_VIEW_SIZE);
                        mSeek.setMax((int)mPlayer.getParam(MediaPlayer.PARAM_MEDIA_DURATION));
                        testPlayerPlay(true);
                    }
                }
                break;
            case MediaPlayer.MSG_OPEN_FAILED: {
                    String str = String.format(getString(R.string.open_video_failed), mURL);
                    Toast.makeText(MainActivity.this, str, Toast.LENGTH_LONG).show();
                }
                break;
            case MediaPlayer.MSG_PLAY_COMPLETED: {
                    if (!mIsLive) finish();
                }
                break;
            case MediaPlayer.MSG_VIDEO_RESIZED: {
                    mVideo.setVisibility(View.INVISIBLE);
                    mHandler.sendEmptyMessage(MSG_UDPATE_VIEW_SIZE);
                }
                break;
            }
        }
    };

    private String readPlayerOpenURL() {
        return mSharedPrefs.getString(KEY_PLAYER_OPEN_URL, DEF_PLAYER_OPEN_URL);
    }

    private void savePlayerOpenURL(String url) {
        SharedPreferences.Editor editor = mSharedPrefs.edit();
        editor.putString(KEY_PLAYER_OPEN_URL, url);
        editor.commit();
    }
}

