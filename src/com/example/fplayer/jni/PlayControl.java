
package com.example.fplayer.jni;

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
    static{
        System.loadLibrary("FPlayer");
    }
    public native void startPlayer(String inputstr, Surface holder);
}
