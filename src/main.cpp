
#include <iostream>
#include <string>
#include <vector>


#include <filesystem>
#include <vector>
#include <json/json.h>
#include <fstream>

#include "geo/silly_geo_utils.h"
#include "string/silly_algorithm.h"
#include "singleton/silly_singleton.h"
#include "encode/silly_encode.h"

// 将文件中的某个字符全部替换成其他的
void replaceBackslashWithForwardslash(const std::string& filename) 
{
	std::ifstream inFile(filename); // 以读取模式打开文件
	if (!inFile.is_open()) 
	{
		std::cout << "Failed to open file: " << filename << std::endl;
		return;
	}

	std::string content;
	std::string line;
	while (std::getline(inFile, line)) 
	{
		// 替换每行中的 \ 为 /
		size_t pos = 0;
		while ((pos = line.find('\\', pos)) != std::string::npos) 
		{
			line.replace(pos, 1, "/");
			pos += 1; // 跳过当前替换后的斜杠字符
		}
		content += line + "\n";
	}
	inFile.close();

	std::ofstream outFile(filename); // 重新以写入模式打开同名文件
	if (!outFile.is_open()) 
	{
		std::cout << "Failed to open file for writing: " << filename << std::endl;
		return;
	}

	outFile << content; // 将替换后的内容写回文件
	outFile.close();

	std::cout << "File '" << filename << "' processed successfully." << std::endl;
}


/// <summary>
/// 初始化配置文件的类
/// </summary>
class TxtToTifConfig : public silly_singleton<TxtToTifConfig>
{
public:
	std::string input_txt_dir;
	std::string output_tif_dir;


	bool read_config(std::string config_path)
	{
		bool status = false;
		std::fstream input;
		input.open(config_path, std::ios::binary | std::ios::in);
		if (input.is_open())
		{
			Json::Reader reader;
			Json::Value root;

			if (reader.parse(input, root))
			{
				input_txt_dir = root["input_txt_dir"].asString();
				output_tif_dir = root["output_tif_dir"].asString();
			}
			status = true;
		}
		else
		{
			std::cout << "error fail open:" << config_path << std::endl;
		}
		input.close();
		return status;
	}
};

struct headerData
{
	int ncols{ 0 };
	int nrows{ 0 };
	double xllcenter{ 0.0 };
	double yllcenter{ 0.0 };
	double cellsize{ 0.0 };
	int NODATA_value{ 0 };
};


/// <summary>
/// (递归,非递归) 的获取指定目录下所有的(文件或者目录)的(绝对路径或者文件名称)
/// </summary>
/// <param name="directoryPath">指定目录</param>
/// <param name="isFile">true:文件，false:目录</param>
/// <param name="returnFullPath">true表示完整路径，false仅表示名称</param>
/// <param name="recursive">false:非递归,true:地推</param>
/// <returns></returns>
std::vector<std::string> collectFileOrDirEntries(const std::string& directoryPath, bool isFile = true, bool returnFullPath = true, bool recursive = false)
{
	std::vector<std::string> entries;
	try
	{
		std::filesystem::path dirPath(directoryPath);
		if (recursive)  // 递归
		{
			for (auto& entry : std::filesystem::recursive_directory_iterator(dirPath))
			{
				if ((isFile && entry.is_regular_file()) || (!isFile && entry.is_directory()))
				{
					if (returnFullPath)
					{
						if (entry.path().extension().string() == ".txt")
						{
							entries.push_back(entry.path().string());
						}
					}
					else
					{
						entries.push_back(entry.path().filename().make_preferred().string());
					}
				}
			}
		}
		else // 非递归
		{
			for (auto& entry : std::filesystem::directory_iterator(dirPath))
			{
				if ((isFile && entry.is_regular_file()) || (!isFile && entry.is_directory()))
				{
					if (returnFullPath)
					{
						if (entry.path().extension().string() == ".txt")
						{
							entries.push_back(entry.path().string());
						}
					}
					else
					{
						entries.push_back(entry.path().filename().make_preferred().string());
					}
				}
			}
		}
	}
	catch (const std::filesystem::filesystem_error& e)
	{
		std::cout << "Error accessing directory: " << e.what() << std::endl;
	}

	return entries;
}

bool TxtToTif(const std::string& input_txt, const std::string& output_tif) 
{
	// 打开TXT文件
	std::ifstream inFile;
	inFile.open(input_txt);
	if (!inFile.is_open()) 
	{
		std::cout << "Failed to open TXT file." << std::endl;
		return false;
	}

	// 记录当前文件指针位置
	std::streampos originalPos = inFile.tellg();
	std::vector<float> data;
	// 检测文件是否包含元数据头部
	std::string firstLine;
	std::getline(inFile, firstLine);
	bool hasHeader = firstLine.find("ncols") != std::string::npos;

	GDALDataset* dataset;
	GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("GTiff");

	if (hasHeader)
	{
		headerData metadata;
		inFile.seekg(originalPos); // 如果需要再次读取第一行，将文件指针移回到开头
		std::string key;
		std::string value;
		// 跳过其余元数据行
		for (int i = 0; i < 6; ++i)
		{
			std::getline(inFile, firstLine);
			std::istringstream iss(firstLine);
			if (!(iss >> key >> value))
			{
				continue; // 如果解析失败，则跳过这一行
			}
			if (key == "ncols")
			{
				metadata.ncols = std::stoi(value);
			}
			else if (key == "nrows")
			{
				metadata.nrows = std::stoi(value);
			}
			else if (key == "xllcenter")
			{
				metadata.xllcenter = std::stod(value);
			}
			else if (key == "yllcenter")
			{
				metadata.yllcenter = std::stod(value);
			}
			else if (key == "cellsize")
			{
				metadata.cellsize = std::stod(value);
			}
			else if (key == "NODATA_value")
			{
				metadata.NODATA_value = std::stoi(value);
			}
		}
		// 创建GDAL Dataset
		dataset = driver->Create(output_tif.c_str(), metadata.ncols, metadata.nrows, 1, GDT_Float32, nullptr);
		if (dataset == nullptr) {
			std::cout << "Failed to create TIFF dataset." << std::endl;
			return false;
		}

		//// 设置地理变换参数
		//double geoTransform[6] = { metadata.xllcenter, metadata.cellsize, 0, metadata.yllcenter, 0, -metadata.cellsize };
		//dataset->SetGeoTransform(geoTransform);

		GDALRasterBand* band = dataset->GetRasterBand(1);
		if (band == nullptr) {
			std::cout << "Failed to get raster band." << std::endl;
			GDALClose(dataset);
			return false;
		}

		data.resize(metadata.nrows * metadata.ncols, static_cast<float>(metadata.NODATA_value));
		int rowIndex = 0;
		while (std::getline(inFile, firstLine))
		{
			if (firstLine.empty() || firstLine.front() == '#') continue; // 跳过空行和注释行
			std::istringstream rowStream(firstLine);
			for (int colIndex = 0; colIndex < metadata.ncols && !rowStream.eof(); ++colIndex)
			{
				float value;
				rowStream >> value;
				data[rowIndex * metadata.ncols + colIndex] = value;
			}
			++rowIndex;
		}

		// 写入数据到TIFF
		CPLErr err = band->RasterIO(GF_Write, 0, 0, metadata.ncols, metadata.nrows, data.data(), metadata.ncols, metadata.nrows, GDT_Float32, 0, 0);
		if (err != CE_None)
		{
			std::cout << "Failed to write data to TIFF." << std::endl;
			GDALClose(dataset);
			return false;
		}
	}
	else
	{

		inFile.seekg(originalPos); // 如果需要再次读取第一行，将文件指针移回到开头
		int lineCount = 0; // 统计文件行数
		int colCount = 0;	//统计文件列数
		bool iscolCount = true;
		while (std::getline(inFile, firstLine))
		{
			if (iscolCount)
			{
				std::istringstream iss(firstLine);
				std::string number;
				// 计算第一行的数据列数
				while (iss >> number)
				{
					++colCount;
				}
				iscolCount = false;
			}
			lineCount++;
		}
		inFile.close();
		inFile.open(input_txt);
		if (!inFile.is_open())
		{
			std::cout << "Failed to open TXT file." << std::endl;
			return false;
		}

		int rowIndex = 0;
		std::string Line;
		while (std::getline(inFile, firstLine))
		{
			if (firstLine.empty() || firstLine.front() == '#') continue; // 跳过空行和注释行
			std::istringstream rowStream(firstLine);
			for (int colIndex = 0; colIndex < colCount && !rowStream.eof(); ++colIndex)
			{
				float value;
				rowStream >> value;
				data.push_back(value);
			}
			++rowIndex;
		}

		int b = 0;
		// 创建GDAL Dataset
		dataset = driver->Create(output_tif.c_str(), colCount, lineCount, 1, GDT_Float32, nullptr);
		if (dataset == nullptr) 
		{
			std::cout << "Failed to create TIFF dataset." << std::endl;
			return false;
		}

		GDALRasterBand* band = dataset->GetRasterBand(1);
		if (band == nullptr) 
		{
			std::cout << "Failed to get raster band." << std::endl;
			GDALClose(dataset);
			return false;
		}

		// 写入数据到TIFF
		CPLErr err = band->RasterIO(GF_Write, 0, 0, colCount, lineCount, data.data(), colCount, lineCount, GDT_Float32, 0, 0);
		if (err != CE_None)
		{
			std::cout << "Failed to write data to TIFF." << std::endl;
			GDALClose(dataset);
			return false;
		}
	}

	inFile.close();
	GDALClose(dataset);
	return true;
}



int main()
{

#ifndef NDEBUG
	std::string configPath = "../../../../config/config.json";
#else
	std::string configPath = "./config/config.json";
#endif
	replaceBackslashWithForwardslash(configPath);

	TxtToTifConfig wic;
	if (!TxtToTifConfig::instance().read_config(configPath))
	{
		std::cout << "error read config: " << configPath << std::endl;
		std::cout << "Please check if the configuration file exists or if the path name in the configuration file has been changed to/" << std::endl;
		return -1;
	}

	geo_utils::init_gdal_env();

	if (!std::filesystem::exists(TxtToTifConfig::instance().input_txt_dir))
	{
		std::cout << "not exists :" << TxtToTifConfig::instance().input_txt_dir << std::endl;
		return -1;
	}

	
	std::vector<std::string> all_path = collectFileOrDirEntries(TxtToTifConfig::instance().input_txt_dir);
	std::map<std::string, std::string> txt_tif_pair;
	std::filesystem::path outputTiffRoot(TxtToTifConfig::instance().output_tif_dir);
	std::filesystem::create_directories(outputTiffRoot);
	for (const auto& once : all_path)
	{
		std::filesystem::path file(once);
		std::string tif_name = file.filename().stem().string();
		tif_name += ".tif";
		std::filesystem::path tempPath(outputTiffRoot);
		tempPath.append(tif_name);
		txt_tif_pair[once] = tempPath.string();

	}
	for (const auto& [input_txt, output_tif] : txt_tif_pair)
	{
		if (TxtToTif(input_txt, output_tif))
		{
			std::cout << "succeed: " << output_tif << std::endl;
		}
		else
		{
			std::cout << "failed: " << output_tif << std::endl;
		}
	}

	geo_utils::destroy_gdal_env();

}


