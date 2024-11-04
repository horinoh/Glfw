#pragma once

#include <bitset>
//#include <format>
#include <sstream>
#include <thread>
#include <mutex>
#include <span>
#include <functional>

#ifdef _WIN64
#pragma warning(push)
#pragma warning(disable : 4201)
#else
#pragma GCC diagnostic push
//#pragma GCC diagnostic ignored ""
#endif
#include <hailo/hailort.h>
#ifdef _WIN64
#pragma warning(pop)
#else
#pragma GCC diagnostic pop
#endif

class Hailo
{
public:
	//!< カメラ画像を取得するのに必要な、cv::VideoCapture() へ引数 (文字列) を作成
	//!< (OpenCV では BGR なので "format=BGR" を指定する必要がある)
	static std::string GetLibCameGSTStr(const int Width, const int Height, const int FPS) {
		//return std::format("libcamerasrc ! video/x-raw, width={}, height={}, framerate={}/1, format=BGR, ! appsink", Width, Height, FPS);
		std::stringstream SS;
		SS << "libcamerasrc ! video/x-raw, width=" << Width << ", height=" << Height << ", framerate=" << FPS << "/1, format=BGR, ! appsink";
		return SS.str();
	}

	virtual void Start(std::string_view HefFile, std::string_view CapturePath, std::function<bool()> Loop) {
		Flags.set(static_cast<size_t>(FLAGS::IsRunning));
		Flags.set(static_cast<size_t>(FLAGS::HasInput));

		//!< デバイス
		hailo_device Device;
		hailo_create_pcie_device(nullptr, &Device);

		//!< ネットワーク
		hailo_hef Hef;
		hailo_create_hef_file(&Hef, std::data(HefFile));
		hailo_configure_params_t ConfigureParams;
		hailo_init_configure_params(Hef, HAILO_STREAM_INTERFACE_PCIE, &ConfigureParams);

		std::vector<hailo_configured_network_group> ConfiguredNetworkGroups(HAILO_MAX_NETWORK_GROUPS);
		size_t Count = std::size(ConfiguredNetworkGroups);
		hailo_configure_device(Device, Hef, &ConfigureParams, std::data(ConfiguredNetworkGroups), &Count);
		ConfiguredNetworkGroups.resize(Count);

		auto& CNG = ConfiguredNetworkGroups.front();

		//!< 入出力パラメータ
		std::vector<hailo_input_vstream_params_by_name_t> InputVstreamParamsByName(HAILO_MAX_STREAM_NAME_SIZE);
		Count = std::size(InputVstreamParamsByName);
		hailo_make_input_vstream_params(CNG, true, HAILO_FORMAT_TYPE_UINT8, std::data(InputVstreamParamsByName), &Count);
		InputVstreamParamsByName.resize(Count);

		std::vector<hailo_output_vstream_params_by_name_t> OutputVstreamParamsByName(HAILO_MAX_STREAM_NAME_SIZE);
		Count = std::size(OutputVstreamParamsByName);
		hailo_make_output_vstream_params(CNG, false, HAILO_FORMAT_TYPE_FLOAT32, std::data(OutputVstreamParamsByName), &Count);
		OutputVstreamParamsByName.resize(Count);

		//!< 入出力 (入力へ書き込むと AI に処理されて出力に返る)
		hailo_input_vstream InputVstreams;
		hailo_create_input_vstreams(CNG, std::data(InputVstreamParamsByName), std::size(InputVstreamParamsByName), &InputVstreams);

		hailo_output_vstream OutputVstreams;
		hailo_create_output_vstreams(CNG, std::data(OutputVstreamParamsByName), std::size(OutputVstreamParamsByName), &OutputVstreams);

		//!< AI ネットワークのアクティベート
		hailo_activated_network_group ActivatedNetworkGroup;
		hailo_activate_network_group(CNG, nullptr, &ActivatedNetworkGroup);

		//!< 推論開始
		Inference(InputVstreams, OutputVstreams, CapturePath);

		//!< ループ
		do {
			Flags[static_cast<size_t>(FLAGS::IsRunning)] = Loop();
		} while (Flags.all());

		//!< 終了、スレッド同期
		Join();
	}

	//!< 継承クラスでオーバーライド
	virtual void Inference([[maybe_unused]] hailo_input_vstream& In, [[maybe_unused]] hailo_output_vstream& Out, [[maybe_unused]] std::string_view CapturePath) {}

	//!< スレッド同期
	void Join() {
		for (auto& i : Threads) {
			i.join();
		}
	}

protected:
	enum class FLAGS : size_t {
		IsRunning,
		HasInput,
	};
	std::bitset<2> Flags;
	std::vector<std::thread> Threads;
};

#ifdef USE_CV
class DepthEstimation : public Hailo
{
private:
	using Super = Hailo;

public:
	virtual void Inference(hailo_input_vstream& In, hailo_output_vstream& Out, std::string_view CapturePath) override {
		//!< AI 入力スレッド
		Threads.emplace_back([&]() {
			cv::VideoCapture Capture(std::data(CapturePath));
			std::cout << Capture.get(cv::CAP_PROP_FRAME_WIDTH) << " x " << Capture.get(cv::CAP_PROP_FRAME_HEIGHT) << " @ " << Capture.get(cv::CAP_PROP_FPS) << std::endl;

			hailo_vstream_info_t Info;
			hailo_get_input_vstream_info(In, &Info);
			const auto& Shape = Info.shape;
			size_t FrameSize;
			hailo_get_input_vstream_frame_size(In, &FrameSize);

			cv::Mat InAI;
			//!< スレッド自身に終了判断させる
			while (Flags.all()) {
				//!< 入力と出力がズレていかないように同期する
				if (InFrameCount > OutFrameCount) { continue; }
				++InFrameCount;

				//!< フレームを取得
				Capture >> ColorMap;
				if (ColorMap.empty()) {
					std::cout << "Lost input" << std::endl;
					Flags.reset(static_cast<size_t>(FLAGS::HasInput));
					break;
				}

				//!< リサイズ
				if (static_cast<uint32_t>(ColorMap.cols) != Shape.width || static_cast<uint32_t>(ColorMap.rows) != Shape.height) {
					cv::resize(ColorMap, InAI, cv::Size(Shape.width, Shape.height), cv::INTER_AREA);
				}
				else {
					InAI = ColorMap.clone();
				}

				//!< AI への入力 (書き込み)
				hailo_vstream_write_raw_buffer(In, InAI.data, FrameSize);
			}
		});

		//!< AI 出力スレッド
		Threads.emplace_back([&]() {
			hailo_vstream_info_t Info;
			hailo_get_output_vstream_info(Out, &Info);
			const auto& Shape = Info.shape;
			size_t FrameSize;
			hailo_get_output_vstream_frame_size(Out, &FrameSize);

			std::vector<uint8_t> OutAI(FrameSize);
			//!< スレッド自身に終了判断させる
			while (Flags.all()) {
				++OutFrameCount;

				//!< 出力を取得
				hailo_vstream_read_raw_buffer(Out, std::data(OutAI), FrameSize);

				//!< OpenCV 形式へ
				const auto CVOutAI = cv::Mat(Shape.height, Shape.width, CV_32F, std::data(OutAI));

				{
					std::lock_guard Lock(OutMutex);

					//!< 深度マップの調整
					DepthMap = cv::Mat(Shape.height, Shape.width, CV_32F, cv::Scalar(0));
					//!< -CVOutAI を指数として自然対数の底 e のべき乗が DepthMap に返る
					cv::exp(-CVOutAI, DepthMap);
					DepthMap = 1 / (1 + DepthMap);
					DepthMap = 1 / (DepthMap * 10 + 0.009);

					double Mn, Mx;
					cv::minMaxIdx(DepthMap, &Mn, &Mx);
					//!< 手前が黒、奥が白
					//DepthMap.convertTo(DepthMap, CV_8U, 255 / (Mx - Mn), -Mn);
					//!< 手前が白、奥が黒 (逆)
					DepthMap.convertTo(DepthMap, CV_8U, -255 / (Mx - Mn), -Mn + 255);
				}
			}
		});
	}

	virtual int GetFps() const { return 30; }

	std::mutex& GetMutex() { return OutMutex; }

	const cv::Mat& GetColorMap() const { return ColorMap; }
	const cv::Mat& GetDepthMap() const { return DepthMap; }

protected:
	int InFrameCount = 0;
	int OutFrameCount = 0;

	std::mutex OutMutex;

	cv::Mat ColorMap;
	cv::Mat DepthMap;
};
#endif
