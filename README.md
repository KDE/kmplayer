KMPlayer, a simple frontend for MPlayer/FFMpeg/Phonon.
It can play DVD/VCD movies, from file or url and from a video device. 
If setup right, KMPlayer can embed inside konqueror. Which means if you click
on a movie file, the movie is played inside konqueror.
It can also embed inside khtml, enabling movie playback inside a html page.
Movie recording using mencoder (part of the mplayer package). No video during recording, but you can always open a new window and play it there.
Broadcasting, http streaming, by using ffserver/ffmpeg. For TV sources, you need v4lctl (part of the xawtv package).

KMPlayer needs KDE Frameworks and a working mplayer/mencoder somewhere in you PATH.
Additonally, for broadcasting, ffserver/ffmpeg/v4lctl also.
For DVD navigation Xine is used. MPlayer has broken dvdnav support. DVDNav is included in libxine and works fine.
Unless you reconfigure kmplayer, both mplayer and ffmpeg should be compiled with liblame for mp3.

KMPlayer doesn't work with all the video drivers that mplayer supports. 
I tested only xv (X Video Extension) and x11 (Image/Shm). If none work, you 
might try the patch for the x11 driver and see if that works for you (not needed anymore for mplayer version >= 0.90-rc4).
You might need to change mimetype settings (Control Center | KDE Components | File Associations) to make sure KMPlayer (or 'Embedded MPlayer for KDE' in embedded tab) is set for all the formats you want to play with KMPlayer.
Also make sure in the 'Embedding' tab 'Show file in embedded viewer' is set.

If you run an older version of MPlayer (0.9x), set 'Post MPlayer 0.90=false' in
.kde/share/config/kmplayer.rc under group [MPlayer]. It will change some of the
mencoder arguments (for recording).

Enjoy!
