package com.github.projectm_android;

import android.content.Context;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import android.content.pm.PackageManager;

import android.util.Log;
import android.view.MotionEvent;
import android.view.WindowManager;

import java.io.File;

public class MainActivity extends AppCompatActivity {
    private static final String TAG = "PM";
    private static final int REQ_AUDIO = 1001;

    private AudioThread audioThread;
    private projectMGLView glSurfaceView;

    public class projectMGLView extends GLSurfaceView {
        private final RendererWrapper mRenderer;

        public projectMGLView(Context context, String assetPath) {
            super(context);
            setEGLContextClientVersion(2);
            mRenderer = new RendererWrapper(assetPath);
            setRenderer(mRenderer);

            // Keep rendering even if audio isn't available (emulator often provides silence).
            setRenderMode(GLSurfaceView.RENDERMODE_CONTINUOUSLY);
        }

        @Override
        public boolean onTouchEvent(MotionEvent e) {
            mRenderer.NextPreset();
            return true;
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        String assetPath = new File(getCacheDir(), "projectM").toString();
        glSurfaceView = new projectMGLView(this, assetPath);
        setContentView(glSurfaceView);

        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    }

    @Override
    protected void onPause() {
        super.onPause();
        if (glSurfaceView != null) glSurfaceView.onPause();

        stopAudioThreadIfRunning();
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (glSurfaceView != null) glSurfaceView.onResume();

        ensureAudioPermissionAndStart();
    }

    private void ensureAudioPermissionAndStart() {
        if (ContextCompat.checkSelfPermission(this, android.Manifest.permission.RECORD_AUDIO)
                == PackageManager.PERMISSION_GRANTED) {
            startAudioThreadIfNeeded();
        } else {
            // Trigger runtime permission prompt
            ActivityCompat.requestPermissions(
                    this,
                    new String[]{android.Manifest.permission.RECORD_AUDIO},
                    REQ_AUDIO
            );
        }
    }

    private void startAudioThreadIfNeeded() {
        if (audioThread != null) return;

        Log.i(TAG, "Starting AudioThread");
        audioThread = new AudioThread();
        audioThread.start();
    }

    private void stopAudioThreadIfRunning() {
        if (audioThread == null) return;

        Log.i(TAG, "Stopping AudioThread");
        try {
            audioThread.stop_recording();
        } catch (Throwable t) {
            Log.w(TAG, "stop_recording() threw", t);
        }

        try {
            audioThread.join(1500);
        } catch (InterruptedException ignored) {
        } catch (Throwable t) {
            Log.w(TAG, "join() threw", t);
        }

        audioThread = null;
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);

        if (requestCode == REQ_AUDIO) {
            boolean granted = grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED;
            if (granted) {
                Log.i(TAG, "RECORD_AUDIO granted");
                startAudioThreadIfNeeded();
            } else {
                Log.w(TAG, "RECORD_AUDIO denied; running without audio input");
                // Visuals still render due to RENDERMODE_CONTINUOUSLY.
            }
        }
    }
}
