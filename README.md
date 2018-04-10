FLHook Discovery 3.1.0
=============

This is the public repository for Discovery Freelancer's FLHook.
The client hook and the server anticheat are not available due to their sensitivity.

The original SVN can be found here: http://forge.the-starport.net/projects/flhookplugin

This project is configured to run under Visual Studio 2017. Everybody can contribute with the Community Edition.
The Windows SDK version is 8.1. 
The compiler is VC141.

Our current starport revision is #267.

If you find old contributions you wish to refactor, you are welcome to do so

IMPORTANT
-------
We are currently in the process of restructuring the solution for ease of use. While it may compile, it is not yet fully finished and tested. We will let you know when we are sure everything works as expected.
If you had an existing copy of the repository, it is very much recommended to start with a new copy and port your changes over as the changes in structure are so major you WILL run into issues.

How to use
-------

Open the solution FLHook.sln
Do not use the solution in DEBUG configuration. Always use RELEASE.

Your compiled plugins are in Binaries/bin-vc14/flhook_plugins.
Sometimes it also like to not copy over the new dll from the plugin's folder into bin-vc14/flhook_plugins for mysterious reasons.

License
-------

Don't claim other's work as your own. Past that, do what you want.
This has been a collaborative project for years and we expect you to respect that.

Contributing
------------

Send pull requests or create issues to discuss
