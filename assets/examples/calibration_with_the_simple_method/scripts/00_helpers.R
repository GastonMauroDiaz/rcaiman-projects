.select_img_files <- function(first_image_id = "7162", images_per_session = 4) {
  first_image <- grep(first_image_id, files)
  img_files <- files[first_image:(first_image + images_per_session - 1)]
  print(img_files)
  img_files
}
