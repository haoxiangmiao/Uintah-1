itcl_class PSECommon_Visualization_GenStandardColorMaps { 
    inherit Module 
    protected exposed
    protected colorMaps
    protected colorMap
    protected alphaMap
    protected curX
    protected curY
    protected selected
    constructor {config} { 
	puts "constructing GenStandardColorMaps"
        set name GenStandardColorMaps 
        set_defaults 
    } 
    
    method set_defaults {} { 
        global $this-tcl_status 
	global $this-mapType
	global $this-resolution
	global $this-minRes
	global $this-nodeList
	global $this-positionList
	set $this-mapType 0
	set $this-resolution 2
	set $this-minRes 2
	set exposed 0
	set colorMap {}
	set selected -1
	set $this-nodeList {}
	set $this-positionList {}
	buildColorMaps
    }   
    
    method buildColorMaps {} {
	set colorMaps {
	    { "Gray" { { 0 0 0 } { 255 255 255 } } }
	    { "Inverse Gray" { { 255 255 255 } { 0 0 0 }}}
	    { "Rainbow" {
		{ 255 0 0}  { 255 102 0}
		{ 255 204 0}  { 255 234 0}
		{ 204 255 0}  { 102 255 0}
		{ 0 255 0}    { 0 255 102}
		{ 0 255 204}  { 0 204 255}
		{ 0 102 255}  { 0 0 255}}}
	    { "Inverse Rainbow" {
		{ 0 0 255}   { 0 102 255}
		{ 0 204 255}  { 0 255 204}
		{ 0 255 102}  { 0 255 0}
		{ 102 255 0}  { 204 255 0}
		{ 255 234 0}  { 255 204 0}
		{ 255 102 0}  { 255 0 0} }}
	    { "Darkhue" {
		{ 0  0  0 }  { 0 28 39 }
		{ 0 30 55 }  { 0 15 74 }
		{ 1  0 76 }  { 28  0 84 }
		{ 32  0 85 }  { 57  1 92 }
		{ 108  0 114 }  { 135  0 105 }
		{ 158  1 72 }  { 177  1 39 }
		{ 220  10 10 }  { 229 30  1 }
		{ 246 72  1 }  { 255 175 36 }
		{ 255 231 68 }  { 251 255 121 }
		{ 239 253 174 }}}
	    { "Inverse Darkhue" {
		{ 239 253 174 }  { 251 255 121 }
		{ 255 231 68 }  { 255 175 36 }
		{ 246 72  1 }  { 229 30  1 }
		{ 220  10 10 }  { 177  1 39 }
		{ 158  1 72 }  { 135  0 105 }
		{ 108  0 114 }  { 57  1 92 }
		{ 32  0 85 }  { 28  0 84 }
		{ 1  0 76 }  { 0 15 74 }
		{ 0 30 55 }  { 0 28 39 }
		{ 0  0  0 } }}
	    { "Lighthue" {
		{ 64  64  64 }  { 64 80 84 }
		{ 64 79 92 }  { 64 72 111 }
		{ 64  64 102 }  { 80 64 108 }
		{ 80 64 108 }  { 92  64 110 }
		{ 118  64 121 }  { 131  64 116 }
		{ 133  64 100 }  { 152  64 84 }
		{ 174  69 69 }  { 179 79  64 }
		{ 189 100  64 }  { 192 152 82 }
		{ 192 179 98 }  { 189 192 124 }
		{ 184 191 151 }}}
	    { "Blackbody" {
		{0 0 0}   {52 0 0}
		{102 2 0}   {153 18 0}
		{200 41 0}   {230 71 0}
		{255 120 0}   {255 163 20}
		{255 204 55}   {255 228 80}
		{255 247 120}   {255 255 180}
		{255 255 255}}}
	    { "Don" {
		{   0  90 255 }    {  51 104 255 }
		{ 103 117 255 }    { 166 131 245 }
		{ 181 130 216 }    { 192 129 186 }
		{ 197 128 172 }    { 230 126  98 }
		{ 240 126  49 }    { 255 133   0 }}}
	}
    }
    
    method getMaps {} {
	set maps {}
	for {set i 0} { $i < [llength $colorMaps]} {incr i} {
	    lappend maps [list [lindex [lindex $colorMaps $i] 0] $i]
	}
	puts "getMaps = $maps"
	return [list $maps]
    }
    method ui {} { 
	global $this-minRes
	global $this-resolution
	global $this-mapType
	
	set w .ui[modname]
	
	if {[winfo exists $w]} { 
	    wm deiconify $w
	    raise $w 
	    return; 
	} 
	
	set type ""
	
	toplevel $w 
	wm minsize $w 200 50 
	
	#set n "$this-c needexecute " 
	set n "$this change"
	
	frame $w.f -relief flat -borderwidth 2
	pack $w.f -side top -expand yes -fill x 
	button $w.f.b -text "close" -command "$this close"
	pack $w.f.b -side left -expand yes -fill y
	
	frame $w.f.f1 -relief sunken -height 40 -borderwidth 2 
	pack $w.f.f1 -side right -padx 2 -pady 2 -expand yes -fill x
	
	canvas $w.f.f1.canvas -bg "#ffffff" -height 40
	pack $w.f.f1.canvas -anchor w -expand yes -fill x

	label $w.l0 -text "Left click to adjust alpha."
	label $w.l1 -text "Right click to remove node."
	label $w.l2 -text "Alpha defaults to 1.0."
	pack $w.l0 $w.l1 $w.l2 -side top -anchor c
	
	frame $w.f2 -relief groove -borderwidth 2
	pack $w.f2 -side left -padx 2 -pady 2 -expand yes -fill both
	
	make_labeled_radio $w.f2.types "ColorMaps" $n top \
	    $this-mapType {
		{ "Gray" 0 } \
		    { "Inverse Gray" 1 } \
		    { "Rainbow" 2} \
		    { "Inverse Rainbow " 3 } \
		    { "Darkhue" 4} \
		    { "Inverse Darkhue" 5} \
		    { "Lighthue" 6} \
		    { "Blackbody" 7} \
		    { "Don" 8} \
		}
	    
	pack $w.f2.types -in $w.f2 -side left
	frame $w.f2.f3 -relief groove -borderwidth 2	
	pack $w.f2.f3 -side left -padx 2 -pady 2 -expand yes -fill both
	label $w.f2.f3.label -text "Resolution"
	pack $w.f2.f3.label -side top -pady 2
	scale $w.f2.f3.s -from [set $this-minRes] -to 255 -state normal \
		-orient horizontal  -variable $this-resolution 
	pack $w.f2.f3.s -side top -padx 2 -pady 2 -expand yes -fill x
	
	bind $w.f2.f3.s <ButtonRelease> "$this update"
	bind $w.f.f1.canvas <Expose> "$this canvasExpose"
	bind $w.f.f1.canvas <Button-1> "$this selectNode %x %y"
	bind $w.f.f1.canvas <B1-Motion> "$this moveNode %x %y"
	bind $w.f.f1.canvas <Button-3> "$this deleteNode %x %y"
	bind $w.f.f1.canvas <ButtonRelease> "$this update"
	$this update
    }
    
    method change {} {
	global $this-minRes
	global $this-resolution
	global $this-mapType
	set w .ui[modname]
	switch  [set $this-mapType] {
	    0  -
	    1  { set $this-minRes 2}
	    2  { set $this-minRes 12}
	    3  { set $this-minRes 19}
	    4  { set $this-minRes 13}
	    default {set $this-minRes 19}
	}
	$w.f2.f3.s configure -from [set $this-minRes]
	$this update
    }

    method selectNode { x y } {
	set w .ui[modname]
	set c $w.f.f1.canvas
	set curX $x
	set curY $y
	
	set selected [$c find withtag current]
	set index [lsearch [set $this-nodeList] $selected]
	if { $index == -1 } {
	    makeNode $x $y
	} 
    }
	

    method makeNode { x y } {
	set w .ui[modname]
	set c $w.f.f1.canvas
	set new [$c create oval [expr $x - 5] [expr $y - 5] \
		     [expr $x+5] [expr $y+5] -outline white \
		     -fill black -tags node]
	set selected $new
	$this nodeInsert $new [list $x $y] $x
	$this drawLines
    }

    method drawLines { } {
	global $this-nodeList
	global $this-positionList
	set w .ui[modname]
	set canvas $w.f.f1.canvas
	$canvas delete line
	set cw [winfo width $canvas]
	set ch [winfo height $canvas]
	set x 0
	set y 0
	for { set i 0 } { $i < [llength [set $this-nodeList]]} {incr i} {
	    set p [lindex [set $this-positionList] $i]
	    $canvas create line $x $y [lindex $p 0] [lindex $p 1] \
		-tags line -fill red
	    set x [lindex $p 0]
	    set y [lindex $p 1]
	}
	$canvas create line $x $y $cw 0  -tags line -fill red
	$canvas raise node line
    }

    method drawNodes { } {
	global $this-nodeList
	global $this-positionList
	set w .ui[modname]
	set c $w.f.f1.canvas
	for {set i 0} { $i < [llength [set $this-nodeList]] } {incr i} {
	    set x [lindex [lindex [set $this-positionList] $i] 0 ]
	    set y [lindex [lindex [set $this-positionList] $i] 1 ]
	    set new [$c create oval [expr $x - 5] [expr $y - 5] \
		     [expr $x+5] [expr $y+5] -outline white \
		     -fill black -tags node]
	    set $this-nodeList [lreplace [set $this-nodeList] $i $i $new]
	}
    }

    method nodeInsert { n p x} {
	global $this-nodeList
	global $this-positionList
	set index 0
	for { set i 0 } { $i < [llength [set $this-nodeList]] } { incr i } { 
	    if { $x < [lindex [lindex [set $this-positionList] $i] 0 ] } {
		break;
	    } else {
		incr index
	    }
	}
	set $this-nodeList [linsert [set $this-nodeList] $index $n]
	set $this-positionList [linsert [set $this-positionList] $index $p]
    }
    
    method moveNode { x y } {
	global $this-nodeList
	global $this-positionList
	set w .ui[modname]
	set c $w.f.f1.canvas
	set cw [winfo width $c]
	set ch [winfo height $c]
	if { $x < 0 } { set x 0 }
	if { $x > $cw } { set x $cw }
	if { $y < 0 } { set y 0 }
	if { $y > $ch } { set y $ch }
	set i [lsearch  [set $this-nodeList] $selected]
	if { $i != -1 } {
	    set l [lindex [set $this-nodeList] $i]
	    $c move $l [expr $x-$curX] [expr $y-$curY]
	    set curX $x
	    set curY $y
	    set $this-nodeList [lreplace [set $this-nodeList] $i $i]
	    set $this-positionList [lreplace [set $this-positionList] $i $i]
	    nodeInsert $l [list $x $y] $x
	    $this drawLines
	}
    }

	
    method deleteNode { x y } {
	global $this-nodeList
	global $this-positionList
	set w .ui[modname]
	set c $w.f.f1.canvas
	set l [$c find withtag current]
	set i [lsearch  [set $this-nodeList] $l]
	if { $i != -1 } {
	    $c delete current
	    set $this-nodeList [lreplace [set $this-nodeList] $i $i]
	    set $this-positionList [lreplace [set $this-positionList] $i $i]
	    $this drawLines
	}
    }
    method update { } {
	$this SetColorMap
	$this redraw
	$this-c setColors [join $colorMap]
	$this-c needexecute
	set selected -1
    }
    
    method close {} {
	set w .ui[modname]
	set exposed 0
	destroy $w
    }
    
    method canvasExpose {} {
	set w .ui[modname]
	
	if { [winfo viewable $w.f.f1.canvas] } { 
	    if { $exposed } {
		return
	    } else {
		set exposed 1
		$this drawNodes
		$this drawLines
		$this redraw
	    } 
	} else {
	    return
	}
    }
    
    method redraw {} {
	set w .ui[modname]
	
	set n [llength $colorMap]
	set canvas $w.f.f1.canvas
	$canvas delete map
	set cw [winfo width $canvas]
	set ch [winfo height $canvas]
	set dx [expr $cw/double($n)] 
	set x 0
	for {set i 0} {$i < $n} {incr i 1} {
	    set color [lindex $colorMap $i]
	    set r [lindex $color 0]
	    set g [lindex $color 1]
	    set b [lindex $color 2]
	    set c [format "#%02x%02x%02x" $r $g $b]
	    set oldx $x
	    set x [expr ($i+1)*$dx]
	    $canvas create rectangle \
		    $oldx 0 $x $ch -fill $c -outline $c -tags map
	}
	set taglist [$canvas gettags all]
	set i [lsearch $taglist line]
	if { $i != -1 } {
	    $canvas lower map line
	}
    }

    method SetColorMap {} {
	global $this-resolution
	global $this-mapType
	set w .ui[modname]
	set colorMap {}
	set map [lindex $colorMaps [set $this-mapType]]
	set currentMap [ lindex $map 1 ]
	set n [llength $currentMap]
	set m [set $this-resolution]
	set frac [expr ($n-1)/double($m-1)]
	for { set i 0 } { $i < $m  } { incr i} {
	    if { $i == 0 } {
		set color [lindex $currentMap 0]
		lappend color [$this getAlpha $i]
	    } elseif { $i == [expr ($m -1)] } {
		set color [lindex $currentMap [expr ($n - 1)]]
		lappend color [$this getAlpha $i]
	    } else {
		set index [expr int($i * $frac)]
		set t [expr ($i * $frac)-$index]
		set c1 [lindex $currentMap $index]
		set c2 [lindex $currentMap [expr $index + 1]]
		set color {}
		for { set j 0} { $j < 3 } { incr j} {
		    set v1 [lindex $c1 $j]
		    set v2 [lindex $c2 $j]
#		    set color [concat $color \
#			       [list [expr int($v1 + $t*($v2 - $v1))]]]
		    lappend color [expr int($v1 + $t*($v2 - $v1))]
		}
		lappend color [$this getAlpha $i]
	    }
#	    set colorMap [concat $colorMap [list $color]]
	    lappend colorMap $color
	}
    }
    
    method getAlpha { index } {
	global nodeList
	global $this-positionList
	set w .ui[modname]
	set m [llength [set $this-nodeList]]
	set canvas $w.f.f1.canvas
	set cw [winfo width $canvas]
	set ch [winfo height $canvas]
	set dx [expr $cw/double([set $this-resolution])] 
	set xpos [expr int($index*$dx) + 0.5 * $dx]
	set x 0.0
	set y 0.0
	set i 0
	for {set i 0} {$i <= $m} {incr i 1} {
	    set newx 0
	    set newy 0
	    if { $i != $m } {
		set newx [lindex [lindex [set $this-positionList] $i] 0]
		set newy [lindex [lindex [set $this-positionList] $i] 1]
	    } else {
		set newy 0
		set newx $cw
	    }

	    if { $xpos < $newx } {
		set frac [expr ( $xpos - $x )/double( $newx - $x )]
		return [expr 1.0 -  (($y + $frac * ($newy - $y))/double($ch))]
	    }
	    set x $newx
	    set y $newy
	}
    }
}

