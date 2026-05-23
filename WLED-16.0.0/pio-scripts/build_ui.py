Import("env")
import os
import shutil

node_ex = shutil.which("node")
# Check if Node.js is installed and present in PATH if it failed, abort the build
if node_ex is None:
    print('\x1b[0;31;43m' + 'Node.js is not installed or missing from PATH html css js will not be processed check https://kno.wled.ge/advanced/compiling-wled/' + '\x1b[0m')
    exitCode = env.Execute("null")
    exit(exitCode)
else:
    if os.path.exists("web-ui/package.json"):
        print('\x1b[6;33;42m' + 'Building Loomx web UI bundle' + '\x1b[0m')
        exitCode = env.Execute("npm ci --prefix web-ui")
        if (exitCode):
          print('\x1b[0;31;43m' + 'npm ci fails in web-ui' + '\x1b[0m')
          exit(exitCode)

        exitCode = env.Execute("npm run build --prefix web-ui")
        if (exitCode):
          print('\x1b[0;31;43m' + 'npm run build fails in web-ui' + '\x1b[0m')
          exit(exitCode)

    # Install the necessary node packages for the pre-build asset bundling script
    print('\x1b[6;33;42m' + 'Installing node packages' + '\x1b[0m')
    env.Execute("npm ci")

    # Call the bundling script
    exitCode = env.Execute("npm run build")

    # If it failed, abort the build
    if (exitCode):
      print('\x1b[0;31;43m' + 'npm run build fails check https://kno.wled.ge/advanced/compiling-wled/' + '\x1b[0m')
      exit(exitCode)
