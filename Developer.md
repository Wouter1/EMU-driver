<img align="right" width="400" src="E-MU_0404_USB.jpg"/>

Building from source code
===================

1. Open the project in Xcode 5.1.1 (to compile the driver for 10.9) or 7.0.1 (for 10.11)
2. Set up the build settings: select the project (the blue EMUUSBAudio icon in top left) and select the "Build Settings" tab
 * select "OSX 10.9" or "OSX 10.11" in the Architectures/Base SDK
 * select the same in the Deployment/OSX Deployment target
 * in the Packaging/Info.plist file set "EMUUSBAudio/EMUUSBAudio-Info.plist" if you selected 10.9, or "EMUUSBAudio/EMUUSBAudio-Info-11.plist" if you selected 10.11.
3. Select Product/Build menu item
4. Select View/Navigators/Project Navigator menu item
5. right click on the  EMUUSBAudio.kext and click "Show in finder"
6. Copy the kext into the directory where the kextInstall script is

After that the kExtInstall script is ready for use (see Installation)



Tech notes
==========
The current version of the 10.9 driver compiles with Xcode 5.1.1; the 10.11 driver compiles with XCode 7.0.1
Original comes from source forge revision 7. http://sourceforge.net/projects/zaudiodrivermac/.
But it did not compile on OSX 10.9 Mavericks. Many hours have been spent on refactoring, documenting, rewriting, cleaning up and adding new code to get it working and to make it work again on El Capitan after Apple messed up their old USB communication layer.

The present version is not in a great shape. I did a major effort to refactor the input side of the code,
it's on the way but not yet polished. Much of the code is still in original state. 
Audio input and output (record and playback) are now working fine.
There are a few small loose ends and I'd like to clean up the code further.




Release with tag
================
Before releasing, the acceptance test should have been run succesfully.

1. Determine and update the version number in the plist.
2. Commit and push the driver to git
3. push a tag to git, as follows 
 * ```git tag -a v3.5.1 9fceb02 -m "Message here"``` or just ```git tag "Message here"``` to tag the current branch
 * ```git push --tags```
