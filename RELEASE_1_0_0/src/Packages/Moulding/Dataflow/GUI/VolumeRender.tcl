itcl_class Moulding_Visualization_VolumeRender {
    inherit Module
    constructor {config} {
        set name VolumeRender
        set_defaults
    }

    method set_defaults {} {
    }

    method ui {} {
        set w .ui[modname]
        if {[winfo exists $w]} {
            raise $w
            return
        }
        toplevel $w
        label $w.row1 -text "This GUI was auto-generated by the Component Wizard."
        label $w.row2 -text {edit the file "/root/PSE/src/Moulding/GUI/VolumeRender.tcl" to modify it.}
        pack $w.row1 $w.row2 -side top -padx 10 -pady 10
    }
}


