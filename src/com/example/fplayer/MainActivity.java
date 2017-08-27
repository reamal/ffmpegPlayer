package com.example.fplayer;

import com.example.fplayer.jni.PlayControl;
import com.example.fplayer.widget.VideoView;

import android.app.Activity;
import android.os.Bundle;
import android.os.Environment;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.View;
import android.view.View.OnClickListener;

public class MainActivity extends Activity implements OnClickListener {

    private PlayControl pControl;
    private VideoView mVideoView;
    private Surface mSurface;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        initView();
        initEvent();
    }

    private void initEvent() {
        pControl = new PlayControl();
        mSurface = mVideoView.getHolder().getSurface();

    }

    private void initView() {
        findViewById(R.id.btn_start_play).setOnClickListener(this);
        findViewById(R.id.btn_stop_play).setOnClickListener(this);
        mVideoView = findViewById(R.id.video_view);
    }

    @Override
    public void onClick(View v) {
        switch (v.getId()) {
            case R.id.btn_start_play:
                String inputStr =
                        Environment.getExternalStorageDirectory().getAbsolutePath() + "/sintel.mp4";
                pControl.startPlayer(inputStr, mSurface);
                break;
            case R.id.btn_stop_play:

                break;

            default:
                break;
        }

    }
}
