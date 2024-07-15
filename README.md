

#  txt_to_tif


程序用于把 A 目录下的所有 txt 文件,处理成 同名的 tif 文件放到 B 目录下


在 config 目录下的 config.json 中设置 需要读取 txt 文件的目录和 生成tif 文件的目录

适配两种类型的txt文件在 txt 文件在 data 目录中有示例


config.json文件中

input_txt_dir:后面的填写 读取 txt 文件的目录 (该目录必须存在)
output_tif_dir:后面的填写 生成 tif 文件的目录,(该目录可以不存在,会自动创建)

注意: 填写目录是要用 "/" 符号区分目录 尽量不要使用 "\"

运行方法:
	双击 txt_to_tif.exe 文件

