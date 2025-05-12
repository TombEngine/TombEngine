# Tomb Engine 

![Logo](https://github.com/MontyTRC89/TombEngine/blob/7c50d26ca898c74978336d41e16ce3ce0c8ecacd/TEN%20logo.png)

*Tomb Engine* (*TEN*) is an open-source custom level engine which aims to abolish limits and fix bugs of the classic Tomb Raider games. It aims to introduce new features, refine old ones, and provide a user-friendly level creation process. Current support includes:
- *Lua* as the native scripting language.
- Many objects from the original series (1-5).
- Support for high framerate, antialiasing, mipmapping, and SSAO.
- Full diagonal geometry support.
- Uncapped map size.
- A streamlined player control scheme.

*Tomb Engine* is used in conjunction with *Tomb Editor*. The repository can be found [here](https://github.com/MontyTRC89/Tomb-Editor).

# Compiling *Tomb Engine*
To compile *TEN*, ensure you have installed:
- *Microsoft Visual Studio*
- *Tomb Editor* (for level creation and testing)

Steps:
1) Clone the repository to your GitHub Desktop.
2) Open `TombEngine.sln`.
4) Compile the solution.
5) Once compiled, create a separate folder to serve as your main *TEN* directory (or create a test *TEN* project using *TombIDE*)
6) Copy everything inside the `Build` folder to the main *TEN* directory.
7) Ensure you have the necessary level data and texture files.
8) In case Windows warns about missing DLLs (bass.dll, etc.), copy the missing DLL files found inside the `Libs` folder to your main `TEN` directory.

*Visual Studio* may warn about NuGet packages. To fix:
1) Delete the `Packages` folder.
2) Go back to *Microsoft Visual Studio*.
3) Right-click on the *TEN* solution in the *Solution Explorer* tab and select "Restore NuGet Packages".
4) If it doesn't help, manually install  `directxtk_desktop_2019` and `Microsoft.XAudio2.Redist` packages via NuGet Package Manager.

Once done, you should be able to build a level with *Tomb Editor* and run it in *TEN*.

# Contributions
Contributions are welcome. If you would like to participate in development to any degree, whether that be through suggestions, bug reports, or code, join our [Discord server](https://discord.gg/h5tUYFmres).

# Disclaimer
Tomb Engine uses modified MIT license for non-commercial use only. For more information, see [license](LICENSE). Tomb Engine is unaffiliated with Core Design, Eidos Interactive, or Embracer Group AB. *Tomb Raider* is a registered trademark of Embracer Group AB. Tomb Engine source code is open to encourage contributions and for study purposes. Tomb Engine team is not responsible for illegal use of this source code alone or in combination with third-party assets or components. This source code is released as-is and continues to be maintained by non-paid contributors in their free time.

