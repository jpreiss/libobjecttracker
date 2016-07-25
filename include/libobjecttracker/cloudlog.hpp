#pragma once

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include "libobjecttracker/object_tracker.h"

#include <chrono>
#include <fstream>
#include <type_traits>

// point cloud log format:
// infinite repetitions of:
// timestamp (milliseconds) : uint32
// cloud size               : uint32
// [x y z, x y z, ... ]     : float32

class PointCloudLogger
{
public:
	PointCloudLogger(std::string file_path) : path(file_path)
	{
	}

	void log(pcl::PointCloud<pcl::PointXYZ>::ConstPtr cloud)
	{
		auto stamp = std::chrono::high_resolution_clock::now();
		if (clouds.empty()) {
			start = stamp;
		}
		auto millis = std::chrono::duration_cast<std::chrono::milliseconds>
			(stamp - start).count();
		timestamps.push_back(millis);
		clouds.push_back(cloud);
	}

	void flush()
	{
		std::ofstream s(path, std::ios::binary | std::ios::out);
		for (size_t i = 0; i < clouds.size(); ++i) {
			write(s, timestamps[i]);
			write(s, (uint32_t)clouds[i]->size());
			for (pcl::PointXYZ const &p : *clouds[i]) {
				static_assert(std::is_same<decltype(p.x), float>::value, "expected float");
				write(s, p.x);
				write(s, p.y);
				write(s, p.z);
			}
		}
	}

private:
	template <typename T>
	void write(std::ofstream &s, T const &t)
	{
		s.write((char const *)&t, sizeof(T));
	}
	std::string path;
	std::chrono::high_resolution_clock::time_point start;
	std::vector<uint32_t> timestamps;
	std::vector<pcl::PointCloud<pcl::PointXYZ>::ConstPtr> clouds;
};

class PointCloudPlayer
{
public:
	void load(std::string path)
	{
		std::ifstream s(path, std::ios::binary | std::ios::in);
		if (!s) {
			throw std::runtime_error("PointCloudPlayer: bad file path.");
		}
		while (s) {
			uint32_t millis = read<uint32_t>(s);
			timestamps.push_back(millis);

			uint32_t size = read<uint32_t>(s);
			clouds.emplace_back(new pcl::PointCloud<pcl::PointXYZ>());
			clouds.back()->resize(size);
			for (uint32_t i = 0; i < size; ++i) {
				float x = read<float>(s);
				float y = read<float>(s);
				float z = read<float>(s);
				(*clouds.back())[i] = pcl::PointXYZ(x, y, z);
			}
		}
	}

	void play(libobjecttracker::ObjectTracker &tracker) const
	{
		for (int i = 0; i < clouds.size(); ++i) {
			auto dur = std::chrono::milliseconds(timestamps[i]);
			std::chrono::high_resolution_clock::time_point stamp(dur);
			tracker.update(stamp, clouds[i]);
		}
	}

private:
	template <typename T>
	T read(std::ifstream &s)
	{
		T t;
		s.read((char *)&t, sizeof(t));
		return t;
	}
	std::vector<uint32_t> timestamps;
	std::vector<pcl::PointCloud<pcl::PointXYZ>::Ptr> clouds;
};