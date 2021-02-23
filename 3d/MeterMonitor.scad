// LCD type
lt = 0; // [0: 0.56inch, 1: 0.36inch]

wall = 1.3; // interior walls
wall_o = 1.9; // outer walls
wall_t = 1.45; // base thickness

/* Hidden */
lid_h = wall_t;

lcd_w = [67, 45.50][lt];
lcd_d = [28, 23.00][lt];
lcd_s_w = [60, 40.35][lt]; // width between screws
lcd_s_d = [22, 17.76][lt];
lcd_s_h = [8.5, 7.0][lt];
lcd_s_dia = [3.1, 3.1][lt];
lcd_a_w = [50, 30][lt]; // width of actual 7-segment area
lcd_a_d = [17.5, 14][lt];
lcd_ex_w = [0, 2][lt];
lcd_ex_d = [2, 4][lt];
lcd_window_offset = [2, (5.43-10.07)/2][lt];
w_extra = [2, 6][lt];
d_extra = [0, 0][lt];

oa_w = 2*lcd_w + 2*lcd_ex_w + w_extra;
oa_d = 3*lcd_d + 3*lcd_ex_d + d_extra;
oa_h = 15;
pad_right = max(2*lcd_window_offset, 0); // padding is asymetrical so not included in oa_w

main();
//translate([-oa_w/2, oa_d/2+wall_o+5, 0]) lid();

module main() {
  difference() {
    union() {
      translate([-oa_w/2-wall_o, -oa_d/2-wall_o, 0])
        cube_bchamfer([oa_w+pad_right+2*wall_o, oa_d+2*wall_o, oa_h+wall_t+lid_h+wall_t/2], r=5, bottom=1);
    } // union
    
    // main cutout
    translate([-oa_w/2, -oa_d/2, wall_t])
      cube_bchamfer([oa_w+pad_right, oa_d, oa_h+e], r=5-wall, bottom=1, top=lid_h);
    // lid cutout
    translate([-oa_w/2,-oa_d/2-wall_o-e,wall_t+oa_h])
      cube_fillet([oa_w+pad_right, oa_d+wall_o+e, lid_h+wall_t+e],
        vertical=[2,2], top=[lid_h,lid_h,0,lid_h], $fn=4);
    
    // LCD holes
    lcd_places() lcd_mount_n();
    
    // USB power hole
    wemos_place() translate([-5.9, -35.2/2-1.2-wall_o-e, 2.3+2-0.5]) 
      cube_fillet([13, 10, 6+1.5], top=[0,1,0,1]);

    //branding();
  } // diff

  lcd_places() lcd_mount_p();
  wemos_place() wemos_mount(2);
}

module lid() {
  ww = oa_w+pad_right+0.2;
  dd = oa_d+wall_o-0.2;
  difference() {
    cube_fillet([ww, dd, lid_h], bottom=[0.5, 0.5, 0, 0.5], vertical=[1.5, 1.5 ,1.5, 1.5]);
    // vent holes
    //translate([15, dd/2, 0])
    //  for (X=[0:3])
    //    translate([X*(ww-30)/3, 0, -e]) rotate(90) cylcyl(d*0.7, 2, lid_h+2*e, $fn=12);
  }
  // lock bump
  //translate([wall_o+wall_t+0.5, dd/2-4*wall, lid_h-e]) cube_fillet([2*wall, 8*wall, 0.6], top=[0.6,0,0.6,0]);
  translate([ww/4, wall_o+wall_t+0.5, lid_h-e]) cube_fillet([ww/2, 2*wall, 0.6], top=[0,0.6,0,0.6]);
}

module lcd_places() {
  // center two
  translate([lcd_w/2+lcd_ex_w,0,0])
    children();
  translate([-lcd_w/2-lcd_ex_w,0,0])
    children();
  // top/bottom
  mirror2([0,1,0]) translate([-lcd_w/2-lcd_ex_w,lcd_d+lcd_ex_d,0])
    children();
}

module lcd_mount_p() {
  mirror2([1,0,0]) mirror2([0,1,0]) translate([lcd_s_w/2, lcd_s_d/2, wall_t]) {
    difference() {
      cylinder(lcd_s_h, d=lcd_s_dia+2*wall, $fn=16);
      translate([0,0,-e]) cylinder(lcd_s_h+2*e, d=lcd_s_dia, $fn=16);
    }
  }
}

module lcd_mount_n() {
  translate([-lcd_a_w/2+lcd_window_offset-wall_t, -lcd_a_d/2-wall_t, -e])
    cube_bchamfer([lcd_a_w+2*wall_t, lcd_a_d+2*wall_t, wall_t+2*e], r=3, top=wall_t, $fn=12);
}

module wemos_place() {
  translate([oa_w/2+pad_right+wall-20, -lcd_d-lcd_ex_d, wall_t]) rotate(90) children();
}

module wemos_mount(extra_h) {
  we_w=25.9;
  we_d=35.2;
  we_h=6;
  we_rad=2.4;
  we_wall=1.2;
  
  difference() {
    translate([-we_w/2-we_wall, -we_d/2-we_wall, 0])
      cube_fillet([we_w+2*we_wall, we_d+2*we_wall, we_h+extra_h],
        vertical=[we_rad+we_wall*0.5855,we_rad+we_wall*0.5855]);
    
    translate([0,0,extra_h]) {
      // Big hole
      translate([-we_w/2, -we_d/2, 3])
        cube_fillet([we_w, we_d, we_h+2*e], vertical=[we_rad,we_rad]);
      // USB jack
      translate([-5.9, -we_d/2-we_wall-e, 2.3]) 
        cube([13, 10, we_h]);
      // Reset button
      translate([we_w/2-e, -we_d/2+1.8, 3.5]) 
        cube([we_wall+2*e, 5, we_h]);
      // Bottom small hole
      translate([-21/2, -31/2, -e])
        cube([21, 31, we_h+2*e]);
      // wifi antenna
      translate([-18/2, we_d/2-(we_d-31)/2-e, 3-1.3]) 
        cube([18, (we_d-31)/2+e, 1.3+e]);
    }
  } // diff
  
  // Grab notches
  translate([0,0,extra_h]) {
    translate([we_w/2,5/2,we_h-0.8/2]) rotate([90]) cylinder(5, d=0.8, $fn=4);
    translate([-we_w/2,5/2,we_h-0.8/2]) rotate([90]) cylinder(5, d=0.8, $fn=4);
  }
}

module branding() {
  translate([lcd_w/2-3,lcd_d+lcd_ex_d,-e]) mirror([1,0,0]) linear_extrude(0.24+e)
    text("HeaterMeter", halign="center", valign="center", font="Impact",
      spacing=1.3, size=7);
}

/********************************** LIBRARY CODE BELOW HERE **********************/

e = 0.01;

module cube_bchamfer(dim, r, top=0, bottom=0, $fn=$fn) {
  // bottom beaded area
  if (bottom != 0) hull(){
    translate([r,r,0]) cylinder(bottom+e, r1=max(0, r-bottom), r2=r, $fn=$fn);
    translate([dim[0]-r,r,0]) cylinder(bottom+e, r1=max(0, r-bottom), r2=r, $fn=$fn);
    translate([r,dim[1]-r,0]) cylinder(bottom+e, r1=max(0, r-bottom), r2=r, $fn=$fn);
    translate([dim[0]-r,dim[1]-r,0]) cylinder(bottom+e, r1=max(0, r-bottom), r2=r, $fn=$fn);
  }
  // center
  translate([0,0,bottom]) hull(){
    translate([r,r,0]) cylinder(dim[2]-top-bottom, r=r, $fn=$fn);
    translate([dim[0]-r,r,0]) cylinder(dim[2]-top-bottom, r=r, $fn=$fn);
    translate([r,dim[1]-r,0]) cylinder(dim[2]-top-bottom, r=r, $fn=$fn);
    translate([dim[0]-r,dim[1]-r,0]) cylinder(dim[2]-top-bottom, r=r, $fn=$fn);
  }
  // top beaded area
  if (top != 0) translate([0,0,dim[2]-top-e]) hull(){
    translate([r,r,0]) cylinder(top+e, r2=max(0, r-top), r1=r, $fn=$fn);
    translate([dim[0]-r,r,0]) cylinder(top+e, r2=max(0, r-top), r1=r, $fn=$fn);
    translate([r,dim[1]-r,0]) cylinder(top+e, r2=max(0, r-top), r1=r, $fn=$fn);
    translate([dim[0]-r,dim[1]-r,0]) cylinder(top+e, r2=max(0, r-top), r1=r, $fn=$fn);
  }
}

module mirror2(dim) {
  // Create both item and mirror of item
  children();
  mirror(dim) children();
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

    fn_V = $fn[0] == undef ? $fn : $fn[0];
    fn_T = $fn[1] == undef ? $fn : $fn[1];
    fn_B = $fn[2] == undef ? $fn : $fn[2];
  
    for (i=[0:3]) {
        if (radius > -1) {
            rotate([0, 0, 90*i]) translate([size[1-j[i]]/2, size[j[i]]/2, 0]) fillet(radius, size[2], fn_V);
        } else {
            rotate([0, 0, 90*i]) translate([size[1-j[i]]/2, size[j[i]]/2, 0]) fillet(vertical[i], size[2], fn_V);
        }
        rotate([90*i, -90, 0]) translate([size[2]/2, size[j[i]]/2, 0 ]) fillet(top[i], size[1-j[i]], fn_T);
        rotate([90*(4-i), 90, 0]) translate([size[2]/2, size[j[i]]/2, 0]) fillet(bottom[i], size[1-j[i]], fn_B);

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