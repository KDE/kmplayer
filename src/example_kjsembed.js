#!/usr/bin/env kjscmd

/**
 * Simple video widget added to your own KJSEmbed application that plays a movie
 */

// Create main view
var mw = new KMainWindow();
var box = new QVBox( mw );
mw.setCentralWidget(box);

var part = Factory.createROPart("application/x-kmplayer", box, "video_win");

/**
 * KJS Bindings for KDE-3.3 also allows passing extra arguments to the part
 * This allows to split of the control panel from the video widget:
 
var part = Factory.createROPart("application/x-kmplayer", "'KParts/ReadOnlyPart' in ServiceTypes", box, "video_win", ["CONSOLE=foo", "CONTROLS=ImageWindow"]);
var part1 = Factory.createROPart("application/x-kmplayer", "'KParts/ReadOnlyPart' in ServiceTypes", box, "control_win", ["CONSOLE=foo", "CONTROLS=ControlPanel"]);

 * The order in which the part are created doesn't really matter. Also on which
 * part openURL is called should not make a difference
 */

/**
 * There are at least two ways to communicate with kmplayer part
 * 1. use the slots of the part (see below), an array of slots is returned by
 *    part.slots()
 * 2. use kmplayer's DCOP interface
 */
 
var stopbutton = new QPushButton( box, 'Stop' );
stopbutton.text = "&Stop";
mw.connect( stopbutton, 'clicked()', part, 'stop()' );
part.openURL( "file:/home/koos/doc/example.avi" );

mw.show();
application.exec();
