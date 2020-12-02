# typed: __STDLIB_INTERNAL

# [`Object`](https://docs.ruby-lang.org/en/2.6.0/Object.html) is the default
# root of all Ruby objects.
# [`Object`](https://docs.ruby-lang.org/en/2.6.0/Object.html) inherits from
# [`BasicObject`](https://docs.ruby-lang.org/en/2.6.0/BasicObject.html) which
# allows creating alternate object hierarchies. Methods on
# [`Object`](https://docs.ruby-lang.org/en/2.6.0/Object.html) are available to
# all classes unless explicitly overridden.
#
# [`Object`](https://docs.ruby-lang.org/en/2.6.0/Object.html) mixes in the
# [`Kernel`](https://docs.ruby-lang.org/en/2.6.0/Kernel.html) module, making the
# built-in kernel functions globally accessible. Although the instance methods
# of [`Object`](https://docs.ruby-lang.org/en/2.6.0/Object.html) are defined by
# the [`Kernel`](https://docs.ruby-lang.org/en/2.6.0/Kernel.html) module, we
# have chosen to document them here for clarity.
#
# When referencing constants in classes inheriting from
# [`Object`](https://docs.ruby-lang.org/en/2.6.0/Object.html) you do not need to
# use the full namespace. For example, referencing `File` inside `YourClass`
# will find the top-level
# [`File`](https://docs.ruby-lang.org/en/2.6.0/File.html) class.
#
# In the descriptions of Object's methods, the parameter *symbol* refers to a
# symbol, which is either a quoted string or a
# [`Symbol`](https://docs.ruby-lang.org/en/2.6.0/Symbol.html) (such as `:name`).
class Object < BasicObject
  include Kernel

  # Returns an integer identifier for `obj`.
  #
  # The same number will be returned on all calls to `object_id` for a given
  # object, and no two active objects will share an id.
  #
  # Note: that some objects of builtin classes are reused for optimization. This
  # is the case for immediate values and frozen string literals.
  #
  # Immediate values are not passed by reference but are passed by value: `nil`,
  # `true`, `false`, Fixnums, Symbols, and some Floats.
  #
  # ```ruby
  # Object.new.object_id  == Object.new.object_id  # => false
  # (21 * 2).object_id    == (21 * 2).object_id    # => true
  # "hello".object_id     == "hello".object_id     # => false
  # "hi".freeze.object_id == "hi".freeze.object_id # => true
  # ```
  sig {returns(Integer)}
  def object_id(); end

  # Yields self to the block and returns the result of the block.
  #
  # ```ruby
  # 3.next.then {|x| x**x }.to_s             #=> "256"
  # "my string".yield_self {|s| s.upcase }   #=> "MY STRING"
  # ```
  #
  # Good usage for `yield_self` is value piping in method chains:
  #
  # ```ruby
  # require 'open-uri'
  # require 'json'
  #
  # construct_url(arguments).
  #   yield_self {|url| open(url).read }.
  #   yield_self {|response| JSON.parse(response) }
  # ```
  #
  # When called without block, the method returns `Enumerator`, which can be
  # used, for example, for conditional circuit-breaking:
  #
  # ```ruby
  # # meets condition, no-op
  # 1.yield_self.detect(&:odd?)            # => 1
  # # does not meet condition, drop value
  # 2.yield_self.detect(&:odd?)            # => nil
  # ```
  sig do
    type_parameters(:X)
      .params(blk: T.proc.params(arg: T.untyped).returns(T.type_parameter(:X)))
      .returns(T.type_parameter(:X))
  end
  def yield_self(&blk); end

  ### `then` is just an alias of `yield_self`. Separately def'd here for easier IDE integration

  # Yields self to the block and returns the result of the block.
  #
  # ```ruby
  # 3.next.then {|x| x**x }.to_s             #=> "256"
  # "my string".yield_self {|s| s.upcase }   #=> "MY STRING"
  # ```
  #
  # Good usage for `yield_self` is value piping in method chains:
  #
  # ```ruby
  # require 'open-uri'
  # require 'json'
  #
  # construct_url(arguments).
  #   yield_self {|url| open(url).read }.
  #   yield_self {|response| JSON.parse(response) }
  # ```
  #
  # When called without block, the method returns `Enumerator`, which can be
  # used, for example, for conditional circuit-breaking:
  #
  # ```ruby
  # # meets condition, no-op
  # 1.yield_self.detect(&:odd?)            # => 1
  # # does not meet condition, drop value
  # 2.yield_self.detect(&:odd?)            # => nil
  # ```
  sig do
    type_parameters(:X)
      .params(blk: T.proc.params(arg: T.untyped).returns(T.type_parameter(:X)))
      .returns(T.type_parameter(:X))
  end
  def then(&blk); end
end
