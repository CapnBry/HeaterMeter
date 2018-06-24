// Total number of probes supported (0 for just an endpiece)
count = 4; // [0,1,2,3,4,5]

// Wall thickness, set to multiple of perimeter width
wall = 2.0;
// Flat thickness
wall_t = 2;

// Overall width of flat
oa_w = 60;
// Overall depth of flat
oa_d = 95;
// Interior height between flats
height = 7.5;
// Center hole ratio
center_ratio = 3;

// Diameter of probe-holding hole
probe_d = 4.75;
// Length of probe holder
probe_l = 25;
// Distance of probe holder offset
probe_off = 7;
// Reduce thickness of probe holder (allow wire to pass more easily)
probe_cut = 1.25;

// Include holes around the perimeter of the flat?
flat_has_holes = true;
// Include the probe holder flat stub on endpiece as well? (can help retain wire)
end_has_stub = true;

/* [Hidden] */
// Center hole width
center_w = oa_w / center_ratio;
// Center hole depth
center_d = (oa_d - oa_w) + center_w;
// Diameter of holes in flat
flat_hole_d = (oa_w - center_w) / 4 - 0.5;

e=0.01;

do_render();

module do_render() {
  if (count == 0)
    half(true);
  else if (count == 1)
    single();
  else if (count == 2)
    complete_2();
  else if (count == 3)
    complete_3();
  else if (count == 4)
    complete_4();
  else if (count == 5)
    complete_5();
}

module single() {
  half(false);
  translate([-oa_w - 1,0,0]) rotate(180) half(true);
}

module complete_2() {
  half(false);
  translate([oa_w + probe_off + probe_d + 1, 0, 0]) rotate(180) half(false);
  translate([0, oa_d + 1,0]) rotate(180) half(true);
}

module complete_3() {
  complete_2();
  translate([oa_w + probe_off + probe_d + 1, oa_d + 1, 0]) rotate(180) half(false);
}

module complete_4() {
  single();
  translate([oa_w + probe_off + probe_d + 1, 0, 0]) rotate(180) half(false);
  translate([0, oa_d + 1, 0]) half(false);
  translate([oa_w + probe_off + probe_d + 1, oa_d + 1, 0]) rotate(180) half(false);
}

module complete_5() {
  complete_4();
  translate([-oa_w - 1, oa_d + 1, 0]) rotate(180) half(false);
}

module rextrude_180(diameter, h) {
  intersection() {
    translate([-diameter/2-e, -e, -e])
      cube([diameter+2*e, diameter/2+e, h+2*e]);
    rotate_extrude()
      children();
  }
}

module half(is_end) {
  probe_x=oa_w/2 + probe_off;
  probe_y=-probe_l - 5; // -oa_w/2 - 0;

  difference() {
    union() {
      flat();
      
      if (!is_end) {
        // probe holster
        intersection() {
          translate([probe_x, probe_y, probe_d/2+wall]) rotate([-90]) {
            difference() {
              union() {
                cylinder(probe_l, d=probe_d+2*wall, $fn=24);
                translate([-probe_d/2-wall,0,0])
                  cube([probe_d+2*wall, probe_d/2+wall, probe_l]);
              }
              translate([0,0,-e]) cylinder(probe_l+2*e, d=probe_d, $fn=16);
            }
          }
          // Keep the height of the cylinder less than the height of the layer
          translate([probe_x-probe_d/2-wall, probe_y, 0])
            cube([probe_d+2*wall, probe_l, height+wall_t-probe_cut]);
        } // interection
      } // if !is_end

      if (!is_end)
        // connect to holster
        translate([center_w/2+wall, probe_y, 0])
          cube([probe_x-center_w/2+probe_d/2, probe_l, wall_t]);
      else if (end_has_stub)
        // connect to holster
        mirror([0,1,0]) 
          translate([center_w/2+wall, probe_y, 0])
            cube([probe_x-center_w/2+probe_d/2, probe_l, wall_t]);

      // lip round
      translate([0,-(center_d-center_w)/2,0]) rotate(180)
        rextrude_180(oa_w, height*4, $fn=46)
        translate([center_w/2,0,0]) lip_poly(is_end, height);
      
      translate([0,(center_d-center_w)/2,0]) 
        rextrude_180(oa_w, height*4, $fn=46)
        translate([center_w/2,0,0]) lip_poly(is_end, height);
      // lip flat
      translate([center_w/2,(center_d-center_w)/2,0]) rotate([90]) 
        linear_extrude(center_d-center_w) lip_poly(is_end, height);
      translate([-center_w/2,-(center_d-center_w)/2,0]) rotate([90,0,180]) 
        linear_extrude(center_d-center_w) lip_poly(is_end, height);
    }  // end union
    
    if (is_end)
      mirror([0,1,0]) flat_holes();
    else
       flat_holes();
    
    // chip away a bit of snap in mating area
    translate([-4/2, -center_d/2-wall-e, wall_t+height-wall])
      cube([4, center_d+2*(wall+e), height*2]);
  } // end diff
}

module flat_hole() {
  rotate(20) {
    cylinder(wall_t+2*e, d=flat_hole_d);
    cube([flat_hole_d/2,flat_hole_d/2,wall_t+2*e]);
  }
}

module flat_holes() {
  flat_hole_offset = (oa_w - center_w)/4 + center_w/2;
  
  if (flat_has_holes) {
    translate([0, -(oa_d-oa_w)/2, 0]) for (rot=[0:180/4:180])
      rotate(-rot) translate([flat_hole_offset,0,-e]) flat_hole();
    translate([0, (oa_d-oa_w)/2, 0]) for (rot=[0:180/4:180])
      rotate(rot) translate([flat_hole_offset,0,-e]) flat_hole();
    translate([flat_hole_offset,0,-e]) flat_hole();
    translate([-flat_hole_offset,0,-e]) rotate(180) flat_hole();
  }
}

module flat() {
  difference() {
    hull() {
      translate([0, -(oa_d-oa_w)/2, 0]) cylinder(wall_t, d=oa_w, $fn=60);
      translate([0, (oa_d-oa_w)/2, 0]) cylinder(wall_t, d=oa_w, $fn=60);
    }
   
    // Center hole
    hull() {
      translate([0, -(center_d-center_w)/2, -e]) cylinder(wall_t+2*e, d=center_w+2, $fn=34);
      translate([0, (center_d-center_w)/2, -e]) cylinder(wall_t+2*e, d=center_w+2, $fn=34);
    }
  }
}

module lip_poly(is_end, interior_h) {
  if (is_end)
    lip_poly_stack_end();
  else
    lip_poly_stack(interior_h);
}

module lip_poly_stack(interior_h) {
  h=interior_h+wall_t;
  t_sock=wall;
  t_plug=t_sock;
  // Back the top snap portion away from the mating area by this amount
  // The lip is only 0.6mm so err needs to be less than this for any engagement to occur
  err_h=0.20;
  // Raise the snap lip this extra amount to account for a non-smooth surface
  // or layer rounding error, 1/2 a layer height is good
  err_v=0.15;

  polygon([
    // origin is bottom center, moving counterclockwise
    [0.5, 0], [t_sock,0], [t_sock, h], [-err_h, h],
    [-err_h, h+1+err_v],  
      [0.6-err_h, h+1+0.6+err_v], [0.6-err_h, h+1+0.6+0.4+err_v],
    [-err_h, h+1+0.8+0.8+err_v],
    [-t_plug-err_h, h+1+0.8+0.8+err_v], [-t_plug-err_h, h], [0, h-t_plug-err_h],
    [0, 1+0.8+0.8], [0.8, 1+0.8], [0, 1], [0, 0.5]
  ]);
}

module lip_poly_stack_end() {
  t_sock = wall;
  stub = wall_t - 1.8;

  polygon([
    [0, 0], [t_sock, 0], 
    // these points define the height, should be 1+0.8+0.8=2.6
    // but cut down to wall_t to be flush with flat(), this means wall_t
    // needs to be at least 1.8 to form a proper-sided socket
    [t_sock, wall_t], [0.5, wall_t], [0, wall_t-0.5], [0, wall_t-1],
    [0.8, wall_t-1-0.8], [0.8-stub, wall_t-1-0.8-stub]
  ]);
}