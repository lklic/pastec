# This file provides backward compatibility for older Nix versions
# that don't support flakes. It imports the development shell from flake.nix.

(import (
  fetchTarball {
    url = "https://github.com/edolstra/flake-compat/archive/master.tar.gz";
    sha256 = "0m6nmi4jb34rykzs3lg1ip7ar95zv9s2mr6sqs6k45idpwsmcbfy";
  }
) {
  src = ./.;
}).shellNix
