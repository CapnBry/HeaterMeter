// preview[view:northeast, tilt:topdiagonal]

/* [Options] */
// Pit probe type
Control_Probe = "Thermocouple"; // [Thermocouple,Thermistor,None]
// Raspberry Pi Model
Pi_Model = "3B/2B/1B+"; // [3B/2B/1B+,Connectorless,1A+,Zero]
// Which case halves
Pieces = "Both"; // [Both,Top,Bottom]
// Include cutouts and mounts for LCD/Buttons
LCD = 1; // [0:None,1:2-line]
// Thickness of side walls (mm) - Set to trace width multiple
wall = 2.5;
/* [Advanced] */
// Thickness of top and bottom faces (mm)
wall_t = 2;
// External corner radius on the body (mm)
body_corner_radius = 4;
// Height of the body edge bead chamfer (mm)
body_chamfer_height = 1.5;
// Corner ear height (mm) - 0 to disable
MouseEarHeight = 0;

/* [Hidden] */
d_off = 1.0; // offset the heatermeter origin from the front edge
w = inch(3.725)+1.2; // overall interior case width
d = inch(3.75)+d_off; // overall interior case depth
// 19.1+ headless Zero
// 22.4 headless Pi3 (limited by nuttrapps interfering with PCB to -6.7)
// 32 standard
h_b = [32-6.7, 32][LCD];  // overall interior case height

probe_centerline = 9.3; // case is split along probe centerline on the probe side
case_split = 12.4;  // and the case split on the other 3 sides

pic_ex = 1.7;
lcd_mount_t = 8.2 - (wall_t - 0.8);

body_chamfer_height_t = body_chamfer_height;
body_chamfer_height_b = body_chamfer_height;

e = 0.01;
is_jig = 0; // generate a jig for soldering LEDs

function inch(x) = x*25.4;

main();

module main()
{
  if (is_jig) {
    intersection() {
      hm43_split();
      translate([wall+6,-d-wall+3,-e]) cube([w-12,d-8,15]);
    }

    difference() {
      cube([w-12,d-8,8 - wall_t]);
      translate([wall,wall,-e]) cube([w-12-2*wall, d-8-2*wall, 8 - wall_t +2*e]);
    }
  } /* is_jig */
  else
    hm43_split();
}

module cube_fillet_chamfer(size,f,c,$fn=32) {
  hull() {
    translate([f,f,c]) linear_extrude(height=size[2]-2*c) minkowski() {
      square([size[0]-2*f, size[1]-2*f]);
      circle(r=f, $fn=$fn);
    }
    translate([f+c,f+c,0]) linear_extrude(height=size[2]) minkowski() {
      square([size[0]-2*(f+c), size[1]-2*(f+c)]);
      circle(r=f, $fn=$fn);
    }
  }
}

module jhole(d, h) {
  translate([0,0,h/2]) cube([2*(wall+pic_ex),d,h], center=true);
}

module phole() {
  dia=4.5;
  // Hole for thermistor probe jack
  rotate([0,90,0]) cylinder(2*wall, d=dia, $fn=16);
  // because 3d print tends to curl up slightly between the probe holes,
  // do not go right to the edge on the final layer (stop 0.25mm short on each side)
  translate([0,-(dia+0.5)/2, -0.3]) cube([2*wall, dia+0.5, 0.6]);
}

module screwhole() {
  translate([0,0,-e]) cylinder(3.5, d=6, $fn=18);
  translate([0,0,3+0.4]) cylinder(h_b-3.5-0.4+wall_t, d=3.4, $fn=18);
  translate([0.0,0,h_b+wall_t-lcd_mount_t/2-e])
    cylinder(lcd_mount_t/2+e, d=(2.9*2)/sin(60), $fn=6);
}

module pic_ex_cube() {
  translate([0,33.75,2])
    cube_fillet([pic_ex+e, 59.5, 20.8], 
      vertical=[0, pic_ex, pic_ex, 0],
      top=[0,pic_ex,0,0],
      bottom=[0,pic_ex,0,0]);
}

module screw_pimount() {
  cylinder(wall_t+2.3, d=6.4, $fn=18);
  // alignment nubbies
  //cylinder(wall_t+2.3+1.4, d=2.5, $fn=12);
}

module screw_keyhole() {
  d=6.2;
  d2=3.5;
  off=4.15; // ideally would be D but runs into edge of case
  h=3;
  
  cylinder(wall_t+2*e, d=d, $fn=18);
  translate([-d2/2,0,0]) cube([d2,off,wall_t+2*e]);
  translate([0,off,0]) {
    cylinder(wall_t+2*e, d=d2, $fn=18);
    translate([0,0,wall_t+e]) cylinder(h+e, d=d, $fn=18);
  }
}

module screw_keyhole_p() {
  d=6.2+wall;
  h=2.3;
  
  difference() {
    cylinder(wall_t+h, d=d, $fn=18);
    translate([0,0,-e]) cylinder(wall_t+h+2*e, d=d-wall, $fn=18);
    translate([-d,0,-e]) cube([d*2, d/2, wall_t+h+2*e]);
  }
}

module btn_rnd() {
  dia = is_jig ? 6.0 : 7.2;
  cylinder(wall_t+2*e, d1=dia, d2=dia+1.5*wall_t, $fn=24);
  translate([-6.5,-6.5,0]) cube([13,13,0.75+e]);
}

module tc_plusminus() {
  T=0.75+e;
  H=2;
  W=6;
  // Minus
  translate([0,2,0]) 
    cube_fillet([T, W, H], top=[0,0,0,T]);
  translate([0,-(2+W),0]) {
    cube_fillet([T, W, H], top=[0,0,0,T]);
    translate([0,(W-H)/2,(H-W)/2])
      cube_fillet([T, H, W], top=[0,0,0,T]);
  }
}

module led_hole() {
  dia = is_jig ? 3.2 : 3.4;
  cylinder(wall_t+2*e, d1=dia, d2=dia, $fn=16);
}

module nuttrap() {
  ww_w=3;
  ww_d=wall;
  nut_h=3.2;
  nut_ingress = 5.7;
  nut_d = nut_ingress / sin(60);
  nut_ingress_off = nut_ingress/sqrt(3)/2;
  oa_h=wall_t+0.4+nut_h+wall_t+[0,6.7][LCD];

  // bottom half for M3 socket cap screw (flush)
  difference() {
    translate([-5.5/2-ww_w, -nut_ingress/2-d_off-e, 0])
      cube_fillet([5.5+2*ww_w, nut_ingress+d_off+ww_d, 6], vertical=[3.4,3.4], $fn=20);
    // socket cap
    translate([0,0,-e]) cylinder(3.5, d=6, $fn=18);
    // screw shaft
    translate([0,0,3.5+0.3]) cylinder(oa_h-3, d=3.4, $fn=18);
  }
  
  // top half M3 nut trap
  translate([0,0,h_b+wall_t-oa_h])
  difference() {
    translate([-(nut_d/2+ww_w), -nut_ingress/2-d_off, 0])
      cube_fillet([nut_d+2*ww_w, nut_ingress+d_off+ww_d, oa_h+e],
        vertical=[3.4,3.4], $fn=20);
    // M3 screw
    translate([0,0,-e]) cylinder(wall_t, d1=4, d2=3.4, $fn=16);
    // nut hole / M3 extra
    translate([0,0,wall_t+0.3]) {
      // nut 2x for an elongated trap
      translate([-0.2,0,0]) cylinder(nut_h*1.5+e, d=nut_d+e/sin(60), $fn=6);
      translate([+0.2,0,0]) cylinder(nut_h*1.5+e, d=nut_d+e/sin(60), $fn=6);
      cylinder(oa_h-wall_t-0.3, d=4, $fn=16);  // M3 with plenty of clearance
      //translate([-50,-50,-100]) cube([100,100,100+e]); // cutaway top
    }
    // nut ingress (sideways trapezoid with wider side at ingress)
    translate([nut_ingress_off,-nut_ingress/2,wall_t+0.3])
       linear_extrude(nut_h+e) polygon([[0,0],
         [nut_d/2+ww_w-nut_ingress_off+0.4+e, -0.8],       
         [nut_d/2+ww_w-nut_ingress_off+0.4+e, nut_ingress+0.8],
         [0,nut_ingress]]);
  }
}

module locklip_p(l, l_offset=0,
  lip_insert_depth=2.0, // how deep V to insert
  lip_v_off=0.2, // extra height before starting insert
  lip_h_off=0.4, // back connector away from mating area
  lip_w=2.5,  // thickness of attachment beam
  insert_inset=[0,0] // inset the insert inside mating area
  ) {
  translate([l_offset,0,0]) rotate([90,0,0]) rotate([0,90,0])
    difference() {
      linear_extrude(height=l-2*l_offset) polygon(points=[
        [0.1, -lip_w-lip_insert_depth-lip_h_off],  // 0.1 to add depth to keep extrusion manifold
        [0.1, 0],
        [-lip_insert_depth-lip_h_off, 0],
        [-lip_insert_depth-lip_h_off, lip_v_off],
        [-lip_h_off, lip_v_off+lip_insert_depth],
        [-lip_insert_depth-lip_h_off, lip_v_off+2*lip_insert_depth,],
        [-lip_w-lip_insert_depth-lip_h_off, lip_v_off+2*lip_insert_depth],
        [-lip_w-lip_insert_depth-lip_h_off, 0]
      ]);
      translate([-lip_w-lip_insert_depth-lip_h_off-e, 0, 0]) {
        if (insert_inset[0] > 0)
          translate([0,0,-e])
            cube([lip_w+lip_insert_depth+lip_h_off+2*e,
              lip_v_off+2*lip_insert_depth+e, insert_inset[0]+e]);
        if (insert_inset[1] > 0)
          translate([0, 0, l-2*l_offset-insert_inset[1]])
            cube([lip_w+lip_insert_depth+lip_h_off+2*e,
              lip_v_off+2*lip_insert_depth+e, insert_inset[1]+e]);
      }
    }
}

module locklip_n(l, l_offset=0,
  lip_insert_depth=2.2, // how deep V to insert
  lip_tip_clip=0.3, // how much to shave off the top tip
  ) {
  translate([l-l_offset,0,0]) rotate([90,0,0]) rotate([0,-90,0]) 
    linear_extrude(height=l-2*l_offset) polygon(points=[
      [0.1, -3*lip_insert_depth],  // 0.1 to add depth to keep extrusion manifold
      [0.1, -lip_tip_clip],
      [-lip_insert_depth+lip_tip_clip, -lip_tip_clip],
      [-e, -lip_insert_depth],
      [-lip_insert_depth, -2*lip_insert_depth]
    ]);
}

module lcd_screw() {
  rotate([180]) difference() {
    //cylinder(lcd_mount_t, d=2.5+3.2, $fn=16);
    cylinder(lcd_mount_t+e, d=2.5, $fn=16);
  }
}

module lcd_mount() {
  // Assuming starting at bottom left screw hole
  //lcd_screw(); 
  //translate([75,0,0]) lcd_screw(); // bottom right hole obscured by PCB
  translate([0,31,0]) lcd_screw();
  translate([75,31,0]) lcd_screw();
}

module lcd_neg() {
  // Assuming starting at bottom left screw hole
  // (Note LCD is not centered vertically on PCB, top is 5.15, bottom is 4.55mm)
  translate([1.0, 1.55, 0])
    cube([73.0, 27.3, lcd_mount_t+wall_t-0.8]); // black bezel inset
  translate([5.5, 7.5, 0])
    cube([64.0, 15.4, lcd_mount_t+wall_t+2*e]); // LCD active area
  translate([4.23, -2.5, 0])
    cube([16*2.54, 5, lcd_mount_t+wall_t-0.8]); // pins cutout
}

module locklip_top_n(split) {
  translate([wall, d+wall, split+wall_t+e])
    rotate([180]) {
      locklip_n(28);
      translate([w-34,0,0]) locklip_n(34);
    }
}

module hm_base() {
  cube_bchamfer([w+2*wall, d+2*wall, h_b+2*wall_t], 
    r=body_corner_radius, top=body_chamfer_height_t, 
    bottom=body_chamfer_height_b, $fn=36);
  // extra thick by Pi connectors
  if (Pi_Model != "Zero" && Pi_Model != "1A+")
    translate([-pic_ex,wall+d_off,wall_t])
      pic_ex_cube();
  // TC +/-
  if (Control_Probe == "Thermocouple")
    translate([w+wall*2-e,wall+d_off+10,wall_t+18]) tc_plusminus();
}

module hm43() {
difference() {
  hm_base();
  
  // Main cutout
  translate([wall, wall, wall_t])
    cube_fillet([w, d, h_b+e], bottom=[2,2,2,2],
    top=[lcd_mount_t/2,lcd_mount_t/2,[wall_t,lcd_mount_t/2][LCD],lcd_mount_t/2],
    vertical=[1,1,1,1]);
  if (Pi_Model != "Zero" && Pi_Model != "1A+")
    translate([wall-pic_ex+e,wall+d_off,wall_t]) pic_ex_cube();

  // Probe jack side
  translate([w+wall*0.5, wall+d_off, wall_t+probe_centerline]) {
    // Probe jacks
    if (Control_Probe == "Thermocouple")
      // TC jack
      translate([0,inch(0.4)-16.5/2,-1.4]) cube([2*wall, 16.5, 6.5]);
    else if (Control_Probe == "Thermistor")
      translate([0,inch(0.28),0]) phole();
    if (Control_Probe != "None") {
      translate([0,inch(0.95)+inch(0.37)*0,0]) phole();
      translate([0,inch(0.95)+inch(0.37)*1,0]) phole();
      translate([0,inch(0.95)+inch(0.37)*2,0]) phole();
    }
  }
  // Pi connector side  
  translate([0,wall+d_off,wall_t]) {
    // Pi connectors
    translate([0,0,5]) {
      if (Pi_Model == "3B/2B/1B+") {
        // ethernet
        translate([0,81.5,-0.5]) jhole(15,13);
        translate([0,81.5,-1.5]) jhole(5,5);
        // USB 0+1
        translate([0,62.75,0]) jhole(13,14.8);
        translate([0,44.75,0]) jhole(13,14.8);
      }
    }
    // HeaterMeter connectors
    translate([0,0,0]) {
      // Blower/Servo output
      translate([0,25,1.1]) jhole(16.7,13);
      // HM power jack
      translate([0,inch(0.2),3.4]) jhole(9.4,11);
    }
  }
  
  // lcd hole
  if (LCD)
    translate([wall+10.7, wall+d_off+inch(2), h_b+wall_t-lcd_mount_t-e]) lcd_neg();
  
  // button holes
  if (LCD) translate([wall+48.7, wall+d_off+inch(1.15), h_b+wall_t-e]) {
    translate([-inch(1.1)/2,0,0]) btn_rnd();  // left
    translate([inch(1.1)/2,0,0]) btn_rnd();   // right
    translate([0,inch(0.9)/2,0]) btn_rnd();   // up
    translate([0,-inch(0.9)/2,0]) btn_rnd();  // down
    // LED holes
    translate([inch(1.3), inch(-0.05), 0]) {
      led_hole();  //red
      translate([0, inch(0.35), 0]) led_hole(); //yellow
      translate([0, inch(0.70), 0]) led_hole(); //green
    }  
  }
  // close screw holes
  translate([wall+inch(0.825)+0.5,wall+d_off+inch(0.1),0]) {
    screwhole();
    translate([inch(2.0),0,0]) screwhole();
  }
  // keyholes
  //translate([wall+24,wall+d_off+39,-e]) {
  //    translate([0,49,0]) screw_keyhole();
  //    translate([58,49,0]) screw_keyhole();
  //}
}  // END OF DIFFERENCE

  // Pi mounting screws
  translate([wall+24,wall+d_off+39,0]) {
    translate([0,0,0]) screw_pimount();
    translate([58,0,0]) screw_pimount();
    if (Pi_Model == "Zero") {
      translate([0,23,0]) screw_pimount();
      translate([58,23,0]) screw_pimount();
    }
    else {
      translate([0,49,0]) screw_pimount();
      translate([58,49,0]) screw_pimount();
      //translate([0,49,0]) screw_keyhole_p();
      //translate([58,49,0]) screw_keyhole_p();
    }
  }

  if (Pi_Model != "Zero") {
    // Pi right edge stop
    translate([wall+w-9.5, wall+d_off+35, wall_t])
      difference() {
        cube_fillet([9.5,d-d_off-35,4], vertical=[0,0,10/2], $fn=24);
        // Pi B+ microsd gap
        translate([-e,22,-e]) cube_fillet([5.5,14.5,4+2*e], vertical=[2,0,0,2], $fn=20);
      }
  } // if !Zero
  
  // close nut traps
  if (!is_jig) translate([wall+inch(0.825)+0.5,wall+d_off+inch(0.1),0]) {
      nuttrap();
      translate([inch(2.0),0,0]) nuttrap();
    }
  
  // Top locklip (negative)
  if (Pi_Model == "3B/2B/1B+")
    locklip_top_n(case_split);
  else
    locklip_top_n(probe_centerline);
  
  // LCD mount
  if (LCD) difference() {
    union() {
      // Filled block above LCD hole
      translate([wall, wall+d_off+78, h_b+wall_t-lcd_mount_t])
        cube([w,d-d_off-78,lcd_mount_t+e], 
          bottom=[lcd_mount_t/2,lcd_mount_t/2,0,lcd_mount_t/2]);
      // LCD grab notch
      translate([wall+10.7, wall+d_off+inch(2), h_b+wall_t-lcd_mount_t])
        translate([(77.5-20)/2,34.0-wall_t,-(1.8+wall_t)])
          // 1.8=thickness of LCD pcb
          cube_fillet([20,1.8+wall_t+wall,1.8+wall_t+e], top=[0,0,1.8+wall_t+e],
            vertical=[wall/2,wall/2]);
    }
    translate([wall+10.7, wall+d_off+inch(2), h_b+wall_t-e]) {
      translate([0, 0, -lcd_mount_t]) lcd_neg();
      // vv Keep screw holes where they have been since the beginning
      translate([0, 0.5, 0]) lcd_mount();
    }
  }
  
  if (Pi_Model == "3B/2B/1B+")
    translate([-pic_ex, wall+d_off, wall_t+2]) {
      // USB pillar reinforcements
      translate([0, (44.75+62.75)/2-1, 1.5])
        cube_fillet([pic_ex+wall, 2, 20.8-1.5], bottom=[0,pic_ex,0,2], top=[0,pic_ex]);
      translate([0, 81.5-15/2-3, 1.5])
        cube_fillet([pic_ex+wall, 2.5, 20.8-1.5], bottom=[0,pic_ex,0,2], top=[0,pic_ex]);
      // Near pic_ex fill
      translate([0, 33.75, 0])
        cube_fillet([pic_ex+wall, 3, 20.8], bottom=[0,pic_ex,0,0], top=[0,pic_ex],
        vertical=[0,0,pic_ex]);
      // Far pic_ex fill (near ethernet)
      translate([wall, 33.75+59.5-pic_ex, 1.5])
       cube_fillet([pic_ex, pic_ex+e, 20.8-1.5], bottom=[0,0,0,2], vertical=[0,pic_ex]);
    }
}

module lip_guide(l) {
  guide_offset = 0.2; // how much to recess the edge guides
  guide_h = 1.2; // how tall above the edge to extend
  linear_extrude(height=l) polygon([
    [0,-e], [wall+guide_offset,-e], [0, wall+guide_offset],
    [-guide_h, wall+guide_offset], [-guide_h, guide_offset],
    [0, guide_offset]
  ]);
}

module hm43_bottom_lips(split) {
  translate([wall, wall, split+wall_t]) {
    // bottom locklip (positive)
    translate([0,d,0]) {
      locklip_p(28, insert_inset=[2,0]);
      translate([w-34,0,0]) locklip_p(34-wall, insert_inset=[0,1]);
    }

    // front guide lip
    translate([w/2-9, 0, 0])
      rotate([0,90]) lip_guide(25);
    // left guide lip assortment
    translate([0, d_off+5.25+9.4/2, 0])
      rotate([-90]) rotate(90) mirror([0,1,0]) lip_guide(25.25-5.25-9.4/2-16.7/2);
    *translate([-pic_ex, d_off+52, 0])
      rotate([-90]) rotate(90) mirror([0,1,0]) lip_guide(3.5);
    *translate([-pic_ex, d_off+70, 0])
      rotate([-90]) rotate(90) mirror([0,1,0]) lip_guide(3.5);
  }
  // probe side guide lip
  translate([wall+w, wall+d-45, probe_centerline+wall_t])
    rotate([-90]) rotate(90) lip_guide(40);
}

module mouseears() {
  me_h = MouseEarHeight;
  me_d = 20;
  me_overlap = wall;
  me_outset_bottom = (me_d/2 - body_corner_radius/4 - body_chamfer_height_b - me_overlap) * 0.707;
  me_outset_top = (me_d/2 - body_corner_radius/4 - body_chamfer_height_t - me_overlap) * 0.707;

  // Bottom outside corners
  if (Pieces != "Top") {
    translate([-me_outset_bottom, d+2*wall+me_outset_bottom, 0])
      cylinder(me_h, d=me_d, $fn=24);
    translate([w+2*wall+me_outset_bottom, d+2*wall+me_outset_bottom, 0])
      cylinder(me_h, d=me_d, $fn=24);
  }

  // top outside corners
  if (Pieces != "Bottom") {
    translate([0,-1,me_h]) rotate([180]) {
      translate([w+2*wall+me_outset_top, d+2*wall+me_outset_top, 0])
        cylinder(me_h, d=me_d, $fn=24);
      translate([-me_outset_top, d+2*wall+me_outset_top, 0])
        cylinder(me_h, d=me_d, $fn=24);
    }
  }

  // common corner area
  translate([-me_outset_bottom/0.707, 0, 0])
    cylinder(me_h, d=me_d, $fn=24);
  translate([w+2*wall+me_outset_bottom/0.707, 0, 0])
    cylinder(me_h, d=me_d, $fn=24);
}

module split_volume() {
  if (Pi_Model == "3B/2B/1B+") {
    difference() {
      translate([-pic_ex-e,-e,-e])
        cube([w+pic_ex+2*wall+2*e, d+2*wall+2*e, wall_t+case_split+2*e]);
      translate([w,0,wall_t+probe_centerline])
        cube([3*wall, d+2*wall+2*e, wall_t+probe_centerline+2*e]);
    }
  }
  else
    translate([-w,-d,-e])
      cube([3*w, 3*d, wall_t+probe_centerline+2*e]);
}

module hm43_split() {
  // bottom
  if (Pieces != "Top") translate([0,1,0]) {
    intersection() { 
      hm43();
      split_volume();
    }
    if (Pi_Model == "3B/2B/1B+")
      hm43_bottom_lips(case_split);
    else
      hm43_bottom_lips(probe_centerline);
  } // if include bottom
  
  // top
  if (Pieces != "Bottom") {
    translate([0,-1,h_b+2*wall_t]) rotate([180]) difference() {
      hm43();
      split_volume();
    }
  }  // if include top
  if (MouseEarHeight > 0.0)
    color("silver") mouseears();
}

/**********************                               **********************/
/********************** END OF CASE / LIBRARY FOLLOWS **********************/
/**********************                               **********************/

module cube_bchamfer(dim, r, top=0, bottom=0, $fn=$fn) {
  // bottom beaded area
  if (bottom != 0) hull(){
    translate([r,r,0]) cylinder(bottom, r1=r-bottom, r2=r, $fn=$fn);
    translate([dim[0]-r,r,0]) cylinder(bottom, r1=r-bottom, r2=r, $fn=$fn);
    translate([r,dim[1]-r,0]) cylinder(bottom, r1=r-bottom, r2=r, $fn=$fn);
    translate([dim[0]-r,dim[1]-r,0]) cylinder(bottom, r1=r-bottom, r2=r, $fn=$fn);
  }
  // center
  translate([0,0,bottom]) hull(){
    translate([r,r,0]) cylinder(dim[2]-top-bottom, r=r, $fn=$fn);
    translate([dim[0]-r,r,0]) cylinder(dim[2]-top-bottom, r=r, $fn=$fn);
    translate([r,dim[1]-r,0]) cylinder(dim[2]-top-bottom, r=r, $fn=$fn);
    translate([dim[0]-r,dim[1]-r,0]) cylinder(dim[2]-top-bottom, r=r, $fn=$fn);
  }
  // top beaded area
  if (top != 0) translate([0,0,dim[2]-top]) hull(){
    translate([r,r,0]) cylinder(top, r2=r-top, r1=r, $fn=$fn);
    translate([dim[0]-r,r,0]) cylinder(top, r2=r-top, r1=r, $fn=$fn);
    translate([r,dim[1]-r,0]) cylinder(top, r2=r-top, r1=r, $fn=$fn);
    translate([dim[0]-r,dim[1]-r,0]) cylinder(top, r2=r-top, r1=r, $fn=$fn);
  }
}

module fillet(radius, height=100, $fn=$fn) {
  if (radius > 0) {
    //this creates acutal fillet
    translate([-radius, -radius, -height / 2 - 0.02]) difference() {
        cube([radius * 2, radius * 2, height + 0.04]);
        if ($fn == 0 && (radius == 2 || radius == 3 || radius == 4)) {
            cylinder(r=radius, h=height + 0.04, $fn=4 * radius);
        } else {
            cylinder(r=radius, h=height + 0.04, $fn=$fn);
        }

    }
  }
}

module cube_fillet(size, radius=-1, vertical=[0,0,0,0], top=[0,0,0,0], bottom=[0,0,0,0], center=false, $fn=4){
    if (center) {
        cube_fillet_inside(size, radius, vertical, top, bottom, $fn);
    } else {
        translate([size[0]/2, size[1]/2, size[2]/2])
            cube_fillet_inside(size, radius, vertical, top, bottom, $fn);
    }
}

module cube_negative_fillet(size, radius=-1, vertical=[3,3,3,3], top=[0,0,0,0], bottom=[0,0,0,0], $fn=$fn){
    j=[1,0,1,0];

    for (i=[0:3]) {
        if (radius > -1) {
            rotate([0, 0, 90*i]) translate([size[1-j[i]]/2, size[j[i]]/2, 0]) fillet(radius, size[2], $fn);
        } else {
            rotate([0, 0, 90*i]) translate([size[1-j[i]]/2, size[j[i]]/2, 0]) fillet(vertical[i], size[2], $fn);
        }
        rotate([90*i, -90, 0]) translate([size[2]/2, size[j[i]]/2, 0 ]) fillet(top[i], size[1-j[i]], $fn);
        rotate([90*(4-i), 90, 0]) translate([size[2]/2, size[j[i]]/2, 0]) fillet(bottom[i], size[1-j[i]], $fn);

    }
}

module cube_fillet_inside(size, radius=-1, vertical=[3,3,3,3], top=[0,0,0,0], bottom=[0,0,0,0], $fn=$fn){
    //makes CENTERED cube with round corners
    // if you give it radius, it will fillet vertical corners.
    //othervise use vertical, top, bottom arrays
    //when viewed from top, it starts in upper right corner (+x,+y quadrant) , goes counterclockwise
    //top/bottom fillet starts in direction of Y axis and goes CCW too

    if (radius == 0) {
        cube(size, center=true);
    } else {
        difference() {
            cube(size, center=true);
            cube_negative_fillet(size, radius, vertical, top, bottom, $fn);
        }
    }
}

//cube_fillet([10,10,10]);