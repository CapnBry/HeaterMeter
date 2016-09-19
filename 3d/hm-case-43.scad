// preview[view:northeast, tilt:topdiagonal]

/* [Options] */
// Pit probe type
Control_Probe = "Thermocouple"; // [Thermocouple,Thermistor]
// Raspberry Pi Model
Pi_Model = "3B/2B/1B+"; // [3B/2B/1B+,1A+,Zero]
// Which case halves
Pieces = "Both"; // [Both,Top,Bottom]
// Corner ear height
MouseEarHeight = 0.0;

/* [Hidden] */
function inch(x) = x*25.4;

w = inch(3.75)+0.5; // overall interior case width
d = inch(3.75)-0.5; // overall interior case depth
h_b = 32; // overall interior case height

probe_centerline = 9.0; // case is split along probe centerline

pic_ex = 2;
lcd_mount_t = 7;

wall = 2.0*1.41; // thickness of side walls (*1.41 so rounded corners are this width)
wall_t = 2; // thickness of top and bottom walls
e = 0.01;

echo("WALL is ", wall);
hm43_split();

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
  // Screw hole for bottom of case (including nut)
  //translate([0,0,-e]) cylinder(h_b+wall_t, d=3.4, $fn=18);
  translate([0,0,-e]) {
    cylinder(0.4, d1=7.4, d2=6.6, $fn=6);
    translate([0,0,0.4]) cylinder(3, d=6.6, $fn=6);
  }
  translate([0,0,3+0.2]) cylinder(h_b+2*wall_t, d=3.4, $fn=18);
  translate([0,0,h_b+wall_t]) cylinder(3, d=5.5, $fn=18);
}

module screwhole2() {
  translate([0,0,-e]) cylinder(3, d=6, $fn=18);
  translate([0,0,3+0.4]) cylinder(h_b-3-0.4, d=3.4, $fn=18);
}

module pic_ex_cube() {
  translate([0,33.75,2])
    cube_fillet([pic_ex+e, 59.5, 20.8], 
      vertical=[0, pic_ex, pic_ex, 0],
      top=[0,pic_ex,0,0],
      bottom=[0,pic_ex,0,0]);
}

module screw_pimount() {
  // 2mm height is good for screw holes
  difference() {
    cylinder(h=2.25, d=6.4, $fn=18);
    //cylinder(h=2+e, d=2.5, $fn=16);
  }
}

module btn_rnd(dia=10) {
  cylinder(wall_t+2*e, d1=dia, d2=dia+1.5*wall_t, $fn=24);
  translate([-6.5,-6.5,0]) cube([13,13,0.75+e]);
}

module btn_square(sq=13) {
  translate([-sq/2,-sq/2,0]) cube([sq,sq,wall_t+2*e]);
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
  cylinder(wall_t+2*e, d=3.4, $fn=16);
}

module nuttrap() {
  ww_w=3;
  ww_d=2.7;
  nut_h=2.8;
  nut_ingress = 5.8; //nut_d * sin(60);
  nut_d = nut_ingress / sin(60);
  oa_h=wall_t+0.4+nut_h+wall_t+6.7;

  // bottom half for M3 socket cap screw (flush)
  difference() {
    translate([-5.5/2-ww_w, -nut_ingress/2-e, 0])
      cube_fillet([5.5+2*ww_w, nut_ingress+ww_d*0.8+e, 6], vertical=[3.4,3.4], $fn=20);
    // socket cap
    translate([0,0,-e]) cylinder(3.5, d=6, $fn=18);
    // screw shaft
    translate([0,0,3.5+0.3]) cylinder(oa_h-3, d=3.4, $fn=18);
  }
  
  // top half M3 nut trap
  translate([0,0,h_b+wall_t-oa_h])
  difference() {
    translate([-(nut_d/2+ww_w), -nut_ingress/2-e, 0])
      cube_fillet([nut_d+2*ww_w, nut_ingress+ww_d+e, oa_h],
        vertical=[3.4,3.4], $fn=20);
    // M3 screw
    translate([0,0,-e]) cylinder(wall_t, d1=4, d2=3.4, $fn=16);
    // nut hole / M3 extra
    translate([0,0,wall_t+0.3]) {
      translate([-0.4,0,0]) // nut offset slightly deeper for easier alignment
        cylinder(nut_h*1.5+e, d=nut_d, $fn=6);  // nut
      cylinder(oa_h-wall_t-0.3, d=4, $fn=16);  // M3 with plenty of clearance
    }
    // nut ingress
    translate([0,-nut_ingress/2,wall_t+0.3])
      cube([nut_d/2+ww_w+e,nut_ingress,nut_h+e]);
  }
}

module locklip_p(l, l_offset=1,
  lip_insert_depth=2.0, // how deep V to insert
  lip_v_off=0.2, // extra height before starting insert
  lip_h_off=0.4, // back connector away from mating area
  lip_w=1.5  // thickness of attachment beam
  ) {
  translate([l_offset,0,0]) rotate([90,0,0]) rotate([0,90,0]) 
    linear_extrude(height=l-2*l_offset) polygon(points=[
      [0, -lip_w-lip_insert_depth-lip_h_off],
      [0, 0],
      [-lip_insert_depth-lip_h_off, 0],
      [-lip_insert_depth-lip_h_off, lip_v_off],
      [-lip_h_off, lip_v_off+lip_insert_depth],
      [-lip_insert_depth-lip_h_off, lip_v_off+2*lip_insert_depth,],
      [-lip_w-lip_insert_depth-lip_h_off, lip_v_off+2*lip_insert_depth],
      [-lip_w-lip_insert_depth-lip_h_off, 0]
    ]);
}

module locklip_n(l, l_offset=1,
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
  lcd_screw(); 
  //translate([75,0,0]) lcd_screw(); // bottom right hole obscured by PCB
  translate([0,31,0]) lcd_screw();
  translate([75,31,0]) lcd_screw();
}

module lcd_neg() {
  // Assuming starting at bottom left screw hole
  translate([1.0, 2.1, 0])
    cube([73.0, 26.3, lcd_mount_t+wall_t-0.8]); // black bezel inset
  translate([5.5, 7.8, 0])
    cube([64.0, 15.4, lcd_mount_t+wall_t+2*e]); // LCD active area
  translate([4.23, -2.5, 0])
    cube([16*2.54, 5, lcd_mount_t+wall_t-0.8]); // pins cutout
}

module mouseears() {
  me_h = MouseEarHeight;
  me_d = 15;
  me_outset_bottom = (me_d/4) - wall/2;
  me_outset_top = (me_d/4) - wall;
  
  // Bottom corners
  translate([-me_outset_bottom, -me_outset_bottom, 0])
    cylinder(me_h, d1=me_d, d2=me_d-2*me_h, $fn=24);
  translate([w+2*wall+me_outset_bottom, -me_outset_bottom, 0])
    cylinder(me_h, d1=me_d, d2=me_d-2*me_h, $fn=24);
  translate([w+2*wall+me_outset_bottom, d+2*wall+me_outset_bottom, 0])
    cylinder(me_h, d1=me_d, d2=me_d-2*me_h, $fn=24);
  translate([-me_outset_bottom, d+2*wall+me_outset_bottom, 0])
    cylinder(me_h, d1=me_d, d2=me_d-2*me_h, $fn=24);

  // top corners
  translate([0,0,h_b+2*wall_t-me_h]) {
    translate([-me_outset_top, -me_outset_top, 0])
      cylinder(me_h, d2=me_d, d1=me_d-2*me_h, $fn=24);
    translate([w+2*wall+me_outset_top, -me_outset_top, 0])
      cylinder(me_h, d2=me_d, d1=me_d-2*me_h, $fn=24);
    translate([w+2*wall+me_outset_top, d+2*wall+me_outset_top, 0])
      cylinder(me_h, d2=me_d, d1=me_d-2*me_h, $fn=24);
    translate([-me_outset_top, d+2*wall+me_outset_top, 0])
      cylinder(me_h, d2=me_d, d1=me_d-2*me_h, $fn=24);
  }
}

module hm43() {
difference() {
  union() {
    cube_bchamfer([w+2*wall, d+2*wall, h_b+2*wall_t], 
      r=wall, top=wall/2, bottom=wall/2, $fn=36);
    // extra thick by Pi connectors
    if (Pi_Model != "Zero")
      translate([-pic_ex,wall,wall_t])
        pic_ex_cube();
    // TC +/-
    if (Control_Probe == "Thermocouple")
      translate([w+wall*2-e,wall+10,wall_t+18]) tc_plusminus();
    if (MouseEarHeight > 0.0)
      mouseears();
  }
  translate([wall, wall, wall_t]) cube([w, d, h_b+e]);
  if (Pi_Model != "Zero")
    translate([wall-pic_ex+e,wall,wall_t]) pic_ex_cube();

  // Probe jack side
  translate([w+wall*0.5,wall,wall_t]) {
    // Probe jacks
    translate([0,24.25,probe_centerline]) {
      if (Control_Probe == "Thermocouple")
        // TC jack
        translate([0,inch(-0.55)-16.5/2,-1.4]) cube([2*wall, 16.5, 6.5]);
      else
        translate([0,-17.25,0]) phole();
      translate([0,inch(0.37)*0,0]) phole();
      translate([0,inch(0.37)*1,0]) phole();
      translate([0,inch(0.37)*2,0]) phole();
    }
  }
  // Pi connector side  
  translate([wall*-0.5,wall,wall_t]) {
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
      if (Pi_Model == "1A+")
        translate([0,44.75,0]) jhole(13,7.4);
    }
    // HeaterMeter connectors
    translate([0,0,0]) {
      // Blower/Servo output
      translate([0,25.25,1.1]) jhole(16.7,13);
      // HM power jack
      translate([0,5.25,3.4]) jhole(9.4,11);
    }
  }
  
  // lcd hole
  translate([wall+10.7, wall+52, h_b+wall_t-lcd_mount_t-e]) lcd_neg();
  
  // button holes
  translate([wall+48.7, wall+29.4, h_b+wall_t-e]) {
    translate([-inch(1.1)/2,0,0]) btn_rnd(7.2);  // left
    translate([inch(1.1)/2,0,0]) btn_rnd(7.2);   // right
    translate([0,inch(0.9)/2,0]) btn_rnd(7.2);   // up
    translate([0,-inch(0.9)/2,0]) btn_rnd(7.2);  // down
    // LED holes
    translate([inch(1.3), inch(-0.05), 0]) {
      led_hole();  //red
      translate([0, inch(0.35), 0]) led_hole(); //yellow
      translate([0, inch(0.70), 0]) led_hole(); //green
    }  
  }
  // close screw holes
  translate([wall+inch(0.8)+1.3,wall+2.9,0]) {
    screwhole2();
    translate([inch(2.0),0,0]) screwhole2();
  }
}  // END OF DIFFERENCE

  // Pi mounting screws
  translate([wall+24,wall+d-7,wall_t]) {
    //screw_pimount();
    translate([58,0,0]) screw_pimount();
    translate([58,-49,0]) screw_pimount();
    translate([0,-49,0]) screw_pimount();
    translate([0,0,0]) screw_pimount();
  }
  // Pi right edge stop
  translate([wall+w-10, wall+d-60, wall_t]) {
    difference() {
      cube_fillet([10,60,4], vertical=[0,0,10/2], $fn=24);
      // Pi B+ microsd gap
      translate([-e,22.5,-e]) cube_fillet([6,14,4+2*e], vertical=[2,0,0,2], $fn=20);
    }
  }
  
  // close nut traps
  translate([wall+inch(0.8)+1.3,wall+2.9,0]) {
    nuttrap();
    translate([inch(2.0),0,0]) nuttrap();
  }
  
  // Top locklip (negative)
  translate([wall, d+wall, probe_centerline+wall_t+e])
    rotate([180]) {
      locklip_n(28);
      translate([w-34,0,0]) locklip_n(34);
    }
  
  // LCD mount
  difference() {
    union() {
      // Filled block above LCD hole
      translate([wall, wall+d-16, h_b+wall_t-lcd_mount_t]) cube([w,16,lcd_mount_t+e]);
      // LCD grab notch
      translate([wall+10.7, wall+52, h_b+wall_t-lcd_mount_t])
        translate([(77.5-20)/2,33.5-wall_t,-(1.8+wall_t)])
          // 1.8=thickness of LCD pcb
          cube_fillet([20,1.8+wall_t+wall,1.8+wall_t+e], top=[0,0,1.8+wall_t+e],
            vertical=[wall/2,wall/2]);
    }
    translate([wall+10.7, wall+52, h_b+wall_t-lcd_mount_t-e]) lcd_neg();
    translate([wall+10.7, wall+52, h_b+wall_t-e]) lcd_mount();
  }
  
  if (Pi_Model == "3B/2B/1B+")
    translate([wall-pic_ex, wall, wall_t]) {
      // USB pillar reinforcements
      translate([0, (44.75+62.75)/2-1, 0]) cube([pic_ex+1, 2, h_b]);
      translate([0, 81.5-15/2-3, 0]) cube([pic_ex+1, 2.5, h_b]);
      // Near pic_ex fill
      translate([0, 33.75, 0]) cube([pic_ex+1, 3, h_b]);
      // Far pic_ex fill (near ethernet)
      translate([0, (33.75+59.5)-3, 0]) cube([pic_ex+1, 3, h_b]);
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

module hm43_bottom_lips() {
  translate([wall, wall, probe_centerline+wall_t]) {
    // bottom locklip (postive)
    translate([0,d,0]) {
      locklip_p(28);
      translate([w-34,0,0]) locklip_p(34);
    }

    // front guide lip
    translate([w/2-9, 0, 0])
      rotate([0,90]) lip_guide(30);
    // right guide lip
    translate([w, d-45, 0])
      rotate([-90]) rotate(90) lip_guide(40);
    // left guide lip assortment
    translate([0, 5.25+9.4/2, 0])
      rotate([-90]) rotate(90) mirror([0,1,0]) lip_guide(25.25-5.25-9.4/2-16.7/2);
    *translate([-pic_ex, 52, 0])
      rotate([-90]) rotate(90) mirror([0,1,0]) lip_guide(3.5);
    *translate([-pic_ex, 70, 0])
      rotate([-90]) rotate(90) mirror([0,1,0]) lip_guide(3.5);
  }
}

module hm43_split() {
  half=wall_t+probe_centerline;

  // bottom
  if (Pieces != "Top") {
    intersection() { 
      hm43(); 
      translate([-w,-d,0]) cube([w*3, d*3, half]); 
    }
    hm43_bottom_lips();
  } // if include bottom
  
  // top
  if (Pieces != "Bottom") {
    translate([0,-2,h_b+2*wall_t]) rotate([180]) intersection() {
      hm43();
      translate([-w,-d,half]) cube([w*3, d*3, h_b]); 
    }
  }  // if include top
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