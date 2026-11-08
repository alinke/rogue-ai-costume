package org.pixelcade.arcadejacket

// Demo mode's preview: the jacket's own looks code (ledlink/src/looks.cpp, arcade.cpp),
// compiled into the app (src/main/cpp, shared with the iPhone app), running on a model
// jacket (a 78 + 77 LED front, two 113 LED sleeves). It takes the same commands and
// live data the jacket does, so what's drawn is what the jacket would show.

object PreviewEngine {
    init {
        System.loadLibrary("jacketpreview")
    }

    external fun count(): Int
    /** out = [part (0 front, 1 sleeve), side (-1 wearer's left, 1 right), t (front 0 hem..1 collar, sleeve 0 shoulder..1 cuff)] */
    external fun pixel(i: Int, out: FloatArray)
    external fun command(cmd: Int, a: Int, b: Int, c: Int)
    external fun music(bass: Int, mid: Int, treble: Int, level: Int, beat: Int)
    external fun heart(bpm: Int)
    /** One frame, now: 0xRRGGBB per LED. */
    external fun render(out: IntArray)
}
