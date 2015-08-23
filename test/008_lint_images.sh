#! /bin/bash -xeu

source ${srcdir}/common.sh

for img in ${srcdir_abs}/lint_images/*.bin ${srcdir_abs}/*.bin; do
  ln -s $img
done

for img in *.bin; do

  echo -e '\n\n\n'
  expect_exit 0 $bin --lint --verbose --out 008_output.tcx --from_image $img
  xmllint --schema ${srcdir_abs}/TrainingCenterDatabasev2.xsd --noout 008_output.tcx

  get_xml_tag_count 008_output.tcx Lap
done

