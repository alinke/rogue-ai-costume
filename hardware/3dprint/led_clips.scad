// Sewable LED-strip clips — rogue AI jacket
// Open in OpenSCAD (free) or MakerWorld's Parametric Model Maker.
// Pick a part below, render (F6), export STL. Print in TPU 95A, flat, no supports.

part = "channel";      // [channel, endcap, splice, elbow45, saddle]

strip_w  = 5.0;   // measure your strip!
strip_t  = 2.5;
fit      = 0.4;
wall     = 1.2;
base     = 1.0;
lip_over = 0.5;
lip_t    = 0.6;
clear    = 0.2;
flange_w = 4.0;
flange_t = 0.8;
hole     = 1.5;
pitch    = 4.5;
seg_len  = 60;

a      = (strip_w + fit) / 2;
wo     = a + wall;
lip_lo = base + strip_t + clear;
top    = lip_lo + lip_t;

module profile_solid(len, extra_w = 0, extra_t = 0) {
    ai = a + extra_w/2; woi = ai + wall; fo = woi + flange_w;
    ll = base + strip_t + extra_t + clear; tp = ll + lip_t;
    translate([0, -woi, 0]) cube([len, 2*woi, base]);
    for (s = [-1, 1]) {
        translate([0, s > 0 ? woi : -fo, 0]) cube([len, flange_w, flange_t]);
        translate([0, s > 0 ? ai : -woi, 0]) cube([len, wall, tp]);
        translate([0, s > 0 ? ai - lip_over : -ai, ll]) cube([len, lip_over, lip_t]);
    }
}

module holes(len, extra_w = 0) {
    hc = a + extra_w/2 + wall + flange_w/2;
    n = floor((len - 1) / pitch);
    x0 = (len - (n - 1) * pitch) / 2;
    for (i = [0 : n - 1], s = [-1, 1])
        translate([x0 + i*pitch - hole/2, s*hc - hole/2, -1]) cube([hole, hole, 10]);
}

module channel(len = seg_len, cap = false, extra_w = 0, extra_t = 0) {
    difference() {
        union() {
            profile_solid(len, extra_w, extra_t);
            if (cap) translate([len - wall, -(a + extra_w/2 + wall), 0])
                cube([wall, 2*(a + extra_w/2 + wall), top + extra_t]);
        }
        holes(len, extra_w);
    }
}

// Two 20 mm arms meeting at 45°, with an open pocket at the joint for the
// three-wire solder bridge. Mirror it in the slicer for the opposite turn.
module elbow45() {
    arm = 20;
    pocket = 10;
    difference() {
        union() {
            channel(arm);
            translate([arm, 0, 0]) rotate([0, 0, 45]) channel(arm);
            translate([arm, 0, 0]) cylinder(r = wo + flange_w, h = flange_t, $fn = 48);
            translate([arm, 0, 0]) cylinder(r = pocket/2 + wall, h = top, $fn = 48);
        }
        translate([arm, 0, base]) cylinder(r = pocket/2, h = 10, $fn = 48);
        translate([arm - 1, -a, base]) cube([2, 2*a, 10]);
    }
}

module saddle(len = 8) {
    post = wo; pad = post + 5; h = strip_t + 0.3;
    difference() {
        union() {
            for (s = [-1, 1]) {
                translate([0, s > 0 ? post : -pad, 0]) cube([len, 5, flange_t]);
                translate([0, s > 0 ? a : -post, 0]) cube([len, wall, h + 0.8]);
            }
            translate([0, -post, h]) cube([len, 2*post, 0.8]);
        }
        for (s = [-1, 1]) translate([len/2 - hole/2, s*(post + 2.5) - hole/2, -1]) cube([hole, hole, 10]);
    }
}

if (part == "channel") channel();
if (part == "endcap")  channel(cap = true);
if (part == "splice")  channel(25, extra_w = 3, extra_t = 2);
if (part == "elbow45") elbow45();
if (part == "saddle")  saddle();
