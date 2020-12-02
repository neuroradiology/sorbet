# typed: true

class ComputingProps
  extend T::Sig
  include T::Props

  const :num_ok, Integer, computed_by: :compute_num_ok
  sig {params(n: Integer).returns(Integer)}
  def self.compute_num_ok(n)
    10
  end

  const :missing, Integer, computed_by: :compute_missing
                                      # ^^^^^^^^^^^^^^^^ error: Method `compute_missing` does not exist on `T.class_of(ComputingProps)`
                                      # ^^^^^^^^^^^^^^^^ error: The typechecker was unable to infer the type of the asserted value

  const :num_wrong_value, Integer, computed_by: :compute_num_wrong_value
                                              # ^^^^^^^^^^^^^^^^^^^^^^^^ error: Argument does not have asserted type `Integer`
  sig {params(inputs: T.untyped).returns(String)}
  def self.compute_num_wrong_value(inputs)
    'not_an_integer'
  end

  const :num_wrong_type, Integer, computed_by: :compute_num_wrong_type
  sig {params(inputs: T.untyped).returns(Integer)}
  def self.compute_num_wrong_type(inputs)
    'not_an_integer' # error: Returning value that does not conform to method result type
  end

  const :not_a_symbol, String, computed_by: 'not_a_symbol'
                                          # ^^^^^^^^^^^^^^ error: Value for `computed_by` must be a symbol literal

  symbol_in_variable = :symbol_in_variable
  const :symbol_in_variable, String, computed_by: symbol_in_variable
                                                # ^^^^^^^^^^^^^^^^^^ error: Value for `computed_by` must be a symbol literal

  const :num_unknown_type, Integer, computed_by: :compute_num_unknown_type
                                               # ^^^^^^^^^^^^^^^^^^^^^^^^^ error: The typechecker was unable to infer the type of the asserted value
  def self.compute_num_unknown_type(inputs)
    T.untyped
  end
end
