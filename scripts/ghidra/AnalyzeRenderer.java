// 3D rendering pipeline, shape/sprite system, camera and viewport.
// Dark zone: 0x4B4200-0x4BEDFF (106 KB) -- zero prior coverage.
// Also covers shape loader, scene dispatch, and horizon integration.
// Invoke: run_ghidra.sh AnalyzeRenderer.java
// Output: $FA_PROJECT/output/AnalyzeRenderer.txt

public class AnalyzeRenderer extends FAScript {

    @Override
    public void run() throws Exception {
        openOutput("AnalyzeRenderer");
        header("RENDER -- scene dispatch and render loop");
        dumpSymbolsMatching("renderscene", "_renderscene", "drawscene", "_drawscene",
                "rendworld", "_rendworld", "renderframe", "_renderframe",
                "drawworld", "_drawworld", "renderall", "_renderall");
        header("RENDER -- callers of T_DefaultHorizon (0x4aacf0)");
        dumpCallers(0x004aacf0L);
        header("RENDER -- callers of @G_Tile@32 (0x447aa5)");
        dumpCallers(0x00447aa5L);
        header("SHAPE -- shape load and cache");
        dumpSymbolsMatching("_shload", "shload", "_loadsh", "loadsh", "_shapeinit", "shapeinit",
                "_shrender", "shrender", "_drawsh", "drawsh", "_rendersh", "rendersh",
                "_shcache", "shcache", "_shfree", "shfree");
        searchStrings(new String[]{".SH", ".sh", "wave1", "SH\0"});
        header("SHAPE -- shape manager range 0x4B4200-0x4BEDFF");
        dumpRange(0x004b4200L, 0x004bedffL);
        header("POLY -- polygon submission and rasterizer");
        dumpSymbolsMatching("_polygon", "polygon", "_drawpoly", "drawpoly", "_renderpoly",
                "renderpoly", "_vertex", "vertex", "_triangle", "triangle",
                "_zbuffer", "zbuffer", "_zbuf", "zbuf", "_zwrite", "zwrite");
        header("SPRITE -- sprite and billboard rendering");
        dumpSymbolsMatching("_sprite", "sprite", "_billboard", "billboard",
                "_drawsprite", "drawsprite", "_renderobj2d", "renderobj2d",
                "_particle", "particle", "_explosion", "explosion");
        header("CAMERA -- viewport and projection");
        dumpSymbolsMatching("_camera", "camera", "_viewport", "viewport",
                "_projection", "projection", "_frustum", "frustum",
                "_setcamera", "setcamera", "_lookat", "lookat",
                "_camupdate", "camupdate", "_camfollow", "camfollow");
        searchStrings(new String[]{"camera", "Camera", "viewport", "Viewport"});
        header("CULL -- visibility culling and spatial partitioning");
        dumpSymbolsMatching("_cull", "cull", "_visible", "visible", "_frustumcull",
                "frustumcull", "_occlude", "occlude", "_lod", "lod",
                "_inview", "inview", "_isvisible", "isvisible");
        header("DDRAW -- DirectDraw surface management");
        dumpSymbolsMatching("_ddraw", "ddraw", "_surface", "surface", "_flip", "flip",
                "_blit", "blit", "_vga", "vga", "_screen", "screen",
                "_pageflip", "pageflip", "_backbuffer", "backbuffer");
        searchStrings(new String[]{"DirectDraw", "IDirectDraw", "CreateSurface"});
        header("WR -- WR raster rendering subsystem");
        dumpSymbolsMatching("_wr", "wrsetup", "wrinit", "wrflush", "wrrender",
                "_wrsetremaps", "wrsetremaps", "_wrpoly", "wrpoly");
        header("PIC -- texture/PIC loading for renderer");
        dumpSymbolsMatching("_picload", "picload", "_loadpic", "loadpic",
                "_picrender", "picrender", "_pictexture", "pictexture");
        searchStrings(new String[]{".PIC", ".pic"});
        closeOutput();
    }
}
