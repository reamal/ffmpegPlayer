
package com.example.fplayer.widget;

import android.content.Context;
import android.graphics.PixelFormat;
import android.util.AttributeSet;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

/**
 * ClassName:VideoView <br/>
 * Function: TODO ADD FUNCTION. <br/>
 * Date: 2017年8月27日 下午8:46:09 <br/>
 * 
 * @author Administrator
 * @version
 */
public class VideoView extends SurfaceView {

    public VideoView(Context context) {
        super(context);
        init();
    }

    public VideoView(Context context, AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    public VideoView(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init();
    }

    private void init() {
        // 初始化，SufaceView绘制的像素格式
        SurfaceHolder holder = getHolder();
        holder.setFormat(PixelFormat.RGB_565);
    }
}
