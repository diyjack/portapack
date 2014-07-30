pcb_l = 120;
pcb_w = 75;
pcb_corner_r = 4.0;
pcb_assembly_clearance = 0.5;

standoff_height = 3;
h1_pcb_thickness = 0.060 * 25.4;
board_spacing = 12.7;
pp_pcb_thickness = 0.060 * 25.4;

side_thickness = 1.5;
standoff_drill_depth = standoff_height;
mounting_hole_diameter = 2.0;
standoff_wall_thickness = 3.0;
pp_top_height = 3.5;

bottom_thickness = side_thickness;

exterior_h = bottom_thickness + standoff_height + h1_pcb_thickness + board_spacing + pp_pcb_thickness + pp_top_height;

$fn=50;

module pcb_shape() {
	translate([pcb_corner_r, pcb_corner_r]) minkowski() {
		square([pcb_l - pcb_corner_r * 2, pcb_w - pcb_corner_r * 2]);
		circle(r=pcb_corner_r);
	}
}

module interior_shape() {
	minkowski() {
		pcb_shape();
		circle(r=pcb_assembly_clearance);
	}
}

module interior_volume() {
	linear_extrude(height=50.0) {
		interior_shape();
	}
}

module exterior_shape() {
	minkowski() {
		interior_shape();
		circle(r=side_thickness);
	}
}

module exterior_volume() {
	linear_extrude(height=exterior_h) {
		exterior_shape();
	}
}

module standoff_cutout_corner() {
	minkowski() {
		square([pcb_corner_r * 2, pcb_corner_r * 2], center=true);
		circle(r=pcb_corner_r);
	}
}

module standoff_cutout_middle() {
	union() {
		circle(r=pcb_corner_r, center=true);
		translate([0, pcb_corner_r]) square(size=[pcb_corner_r * 2, pcb_corner_r * 2], center=true);
	}
}

module standoff_cutouts_shape() {
	difference() {
		interior_shape();
		{
			translate([0, 0]) standoff_cutout_corner();
			translate([pcb_l, 0]) standoff_cutout_corner();
			translate([pcb_l, pcb_w]) standoff_cutout_corner();
			translate([0, pcb_w]) standoff_cutout_corner();
			translate([66, pcb_w - pcb_corner_r]) standoff_cutout_middle();
		}
	}
}

module standoff_cutouts_volume() {
	linear_extrude(height=50.0) {
		standoff_cutouts_shape();
	}
}

module sma() {
	rotate([0, 90, 0]) linear_extrude(height=side_thickness + 2.0) circle(r=6.5/2);
}

module sma_slotted() {
	rotate([0, 270, 180]) linear_extrude(height=side_thickness + 2.0) {
		union() {
			circle(r=6.5/2);
			translate([0, -6.5/2]) square([20, 6.5]);
		}
	}
}

module led() {
	rotate([0, 90, 0]) linear_extrude(height=side_thickness + 2.0) circle(r=2.0/2);
}

module switch() {
	rotate([0, 90, 0]) linear_extrude(height=side_thickness + 2.0) circle(r=4.0/2);
}

module switch_slotted() {
	rotate([0, 270, 180]) linear_extrude(height=side_thickness + 2.0) {
		union() {
			circle(r=4.0/2);
			translate([0, -4.0/2]) square([20, 4.0]);
		}
	}
}

module audio() {
	rotate([0, 90, 0]) linear_extrude(height=side_thickness + 2.0) circle(r=6.5/2);
}

module micro_usb() {
	rotate([90, 0, 90]) linear_extrude(height=side_thickness + 2.0) square([9.0, 4.0], center=true);
}

module micro_sd() {
	rotate([90, 0, 90]) linear_extrude(height=side_thickness + 2.0) square([12.0, 1.5], center=true);
}

module micro_sd_slotted() {
	/* Hack to widen until a little dangling bit gets cut off... */
	rotate([90, 0, 90]) linear_extrude(height=side_thickness + 2.0) square([12.0, 1.5 + 10.0], center=true);
}

module mounting_hole() {
	linear_extrude(height=10.0) circle(r=mounting_hole_diameter/2);
}

module drills() {
	/* Top edge */
	translate([-side_thickness - 1.0, 0, 0]) {
		translate([0, pcb_w - 61.000, 0.635]) sma_slotted();
		translate([0, pcb_w - 48.838, 0.300]) led();
		translate([0, pcb_w - 44.266, 0.300]) led();
		translate([0, pcb_w - 39.694, 0.300]) led();
		translate([0, pcb_w - 35.122, 0.300]) led();
		translate([0, pcb_w - 30.550, 0.300]) led();
		translate([0, pcb_w - 17.900, 0.300]) led();
		translate([0, pcb_w - 24.400, 4.010]) switch_slotted();
		translate([0, pcb_w - 11.400, 4.010]) switch_slotted();
		//translate([0, pcb_w - 15.600, board_spacing - 0.75]) micro_sd_slotted();
		/* Hack to widen until a little dangling bit gets cut off... */
		translate([0, pcb_w - 17.000, board_spacing - 0.75 + 5.0]) micro_sd_slotted();
	}
	/* Bottom edge */
	translate([pcb_l - 1.0, 0, 0]) {
		translate([0, pcb_w - 63.000, 0.635]) sma();
		translate([0, pcb_w - 45.000, 0.635]) sma();
		translate([0, pcb_w - 14.800, board_spacing - 2.5]) audio();
		translate([0, pcb_w - 24.000, 1.5]) micro_usb();
	}
	/* Mounting holes */
	translate([0, 0, -(h1_pcb_thickness + standoff_drill_depth)]) {
		translate([4, 4, 0]) mounting_hole();
		translate([4, pcb_w - 4, 0]) mounting_hole();
		translate([pcb_l - 4, pcb_w - 4, 0]) mounting_hole();
		translate([pcb_l - 4, 4, 0]) mounting_hole();
		translate([66, pcb_w - 4, 0]) mounting_hole();
	}
}

difference() {
	translate([0, 0, -(bottom_thickness + standoff_height + h1_pcb_thickness)]) exterior_volume();
	{
		translate([0, 0, -(standoff_height + h1_pcb_thickness)]) standoff_cutouts_volume();
		translate([0, 0, -(h1_pcb_thickness)]) interior_volume();
		drills();
	}
}
