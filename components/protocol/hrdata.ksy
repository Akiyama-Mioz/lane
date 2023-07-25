meta:
  id: hr_data
  title: HR Data
  endian: be
doc: |
  HR data is a pair of band id and heart rate. Nothing fancy.
seq:
  - id: num_pairs
    type: u1
  - id: pairs
    type: pair
    repeat: expr
    repeat-expr: num_pairs

types:
  pair:
    seq:
      - id: band_id
        type: u1
      - id: heart_rate
        type: u1
