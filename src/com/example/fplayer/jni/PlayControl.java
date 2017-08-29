
package com.example.fplayer.jni;

import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;

/**
 * ClassName:PlayControl <br/>
 * Function: TODO ADD FUNCTION. <br/>
 * Date: 2017年8月27日 下午6:30:33 <br/>
 * 
 * @author Administrator
 * @version
 */
public class PlayControl {
    
    private static final String TAG = PlayControl.class.getSimpleName();

    static{
        System.loadLibrary("FPlayer");
    }
    public native void startPlayer(String inputstr, Surface holder);
    
    /**
     * 创建一个AudioTrac对象，用于播放
     * @param nb_channels
     * @return
     */
    public AudioTrack createAudioTrack(int sampleRateInHz, int nb_channels){
        //固定格式的音频码流
        int audioFormat = AudioFormat.ENCODING_PCM_16BIT;
        Log.i(TAG, "nb_channels:"+nb_channels);
        //声道布局
        int channelConfig;
        if(nb_channels == 1){
            channelConfig = android.media.AudioFormat.CHANNEL_OUT_MONO;
        }else if(nb_channels == 2){
            channelConfig = android.media.AudioFormat.CHANNEL_OUT_STEREO;
        }else{
            channelConfig = android.media.AudioFormat.CHANNEL_OUT_STEREO;
        }
        
        int bufferSizeInBytes = AudioTrack.getMinBufferSize(sampleRateInHz, channelConfig, audioFormat);
        
        AudioTrack audioTrack = new AudioTrack(
                AudioManager.STREAM_MUSIC, 
                sampleRateInHz, channelConfig, 
                audioFormat, 
                bufferSizeInBytes, AudioTrack.MODE_STREAM);
        //播放
        //audioTrack.play();
        //写入PCM
        //audioTrack.write(audioData, offsetInBytes, sizeInBytes);
        return audioTrack;
    }
}
