<img align="right" width="400" src="E-MU_0404_USB.jpg"/>

Building from source code
===================

1. Make the architecture choice: "v9" when you target OSX 9  or 10 (pre-Capitan), or "v11" when targeting 11 (El Capitan) and higher.
2. Open the project in Xcode: use XCode 5.1.1 for v9, or XCode 7.0.1 for v11.
2. Set up the build settings: select the project (the blue EMUUSBAudio icon in top left) and select the "Build Settings" tab
 * in the Architectures/Base SDK select "OSX 10.9"for v9, or "OSX 10.11" for v11 
 * select the same in the Deployment/OSX Deployment target
 * in the Packaging/Info.plist file set "EMUUSBAudio/EMUUSBAudio-Info.plist" for v9, or "EMUUSBAudio/EMUUSBAudio-Info-11.plist" for v11.
3. Select Product/Build menu item
4. Select View/Navigators/Project Navigator menu item
5. right click on the  EMUUSBAudio.kext and click "Show in finder"
6. Copy the kext into the EMU-driver/v9 or EMU-driver/v11.

After that the kExtInstall script is ready for use (see Installation)



Tech notes
==========
The current version of the 10.9 driver compiles with Xcode 5.1.1; the 10.11 driver compiles with XCode 7.0.1.
The original comes from source forge revision 7. http://sourceforge.net/projects/zaudiodrivermac/.
But it did not compile on OSX 10.9 Mavericks. Many hours have been spent on refactoring, documenting, rewriting, cleaning up and adding new code to get it working and to make it work again on El Capitan after Apple messed up their old USB communication layer.

The present version is not in a great shape. I did a major effort to refactor the input side of the code,
it's on the way but not yet polished. Much of the code is still in original state. 
Audio input and output (record and playback) are now working fine.
There are a few small loose ends and I'd like to clean up the code further.




Release with tag
================
Before releasing, the acceptance test should have been run succesfully.

1. Determine and update the version number in the plist (both the "bundle version" and the "Bundle version string, short" in both v9 and v11 version).
2. Commit and push the driver to git
3. push a tag to git, as follows 
 * ```git tag "Message"``` to tag the current branch
 * ```git push --tags```
