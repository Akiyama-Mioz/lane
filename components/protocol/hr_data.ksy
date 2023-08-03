meta:
  id: hr_data
  title: HR Data
  endian: be
doc: |
  HR data is a pair of band id and heart rate. Nothing fancy.
seq:
  - id: num_pairs
    type: u1
  - id: updated_id
    type: str
    size: 3
    encoding: UTF-8
    doc: |
        Which band was updated. Only one band is updated at a time.
        You should iterate over the pairs and find the one that matches.
  - id: pairs
    type: pair
    repeat: expr
    repeat-expr: num_pairs

types:
  pair:
    seq:
      - id: band_id
        type: str
        size: 3
        encoding: UTF-8
      - id: heart_rate
        type: u1
