# typed: strict
# enable-packager: true

class Main < PackageSpec
  export_methods A, C
  export_methods B # error: `export_methods` can only be called once in a package
end
