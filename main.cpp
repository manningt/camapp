/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
refer to this documentation: https://github.com/libcamera-org/libcamera/blob/master/Documentation/guides/application-developer.rst
 */

#include <iomanip>
#include <iostream>
#include <memory>
#include <queue>
#include <sys/mman.h>

#include <libcamera/libcamera.h>

#define LOG(level, text) \
	do                    \
	{                     \
		if (level <= 5)    \
			std::clog << "Line_" << __LINE__ << ": " << text << std::endl; \
	} while (0)


// #include <sys/ioctl.h>
#include "event_loop.h"

#define TIMEOUT_SEC 1
#define NUMBER_OF_BUFFERS 4

using namespace libcamera;
static std::shared_ptr<Camera> camera;
static EventLoop loop;

// globals added by Tom
static Stream *stream;  // <- can get this from the buffer?
std::map<Stream *, std::queue<FrameBuffer *>> frame_buffers_;
std::map<FrameBuffer *, std::vector<libcamera::Span<uint8_t>>> mapped_buffers_;
uint8_t *mapped_buffer_ptr[NUMBER_OF_BUFFERS];

enum dimension {WIDTH, HEIGHT};
uint16_t cam_frame[2];


static void processRequest(Request *request)
{
	static uint64_t previous_timestamp;
	LOG(4, "Request completed: " << request->toString());
	// std::cout << std::endl
	// 	  << "Request completed: " << request->toString() << std::endl;

	/* -
	 * When a request has completed, it is populated with a metadata control
	 * list that allows an application to determine various properties of
	 * the completed request. This can include the timestamp of the Sensor
	 * capture, or its gain and exposure values, or properties from the IPA
	 * such as the state of the 3A algorithms.
	 *
	 * ControlValue types have a toString, so to examine each request, print
	 * all the metadata for inspection. A custom application can parse each
	 * of these items and process them according to its needs.
	 */

	if (1) {
		const ControlList &requestMetadata = request->metadata();
		for (const auto &ctrl : requestMetadata) {
			const ControlId *id = controls::controls.at(ctrl.first);
			const ControlValue &value = ctrl.second;

			if (/*1 || */ id->name() == "ExposureTime")
			{
				std::cout << "\t" << id->name() << " = " << value.toString()
					<< std::endl;
			}
		}
	}

	/*
	 * Each buffer has its own FrameMetadata to describe its state, or the
	 * usage of each buffer. While in our simple capture we only provide one
	 * buffer per request, a request can have a buffer for each stream that
	 * is established when configuring the camera.
	 *
	 * This allows a viewfinder and a still image to be processed at the
	 * same time, or to allow obtaining the RAW capture buffer from the
	 * sensor along with the image as processed by the ISP.
	 */
	const Request::BufferMap &buffers = request->buffers();
	for (auto bufferPair : buffers) {
		// Stream *stream = bufferPair.first;
		// std::cout << "processRequest stream: "
		//   << stream->configuration << std::endl;

		FrameBuffer *buffer = bufferPair.second;
		const FrameMetadata &metadata = buffer->metadata();

		if (previous_timestamp != 0)
			std::cout << " seq: " << std::setw(4) << std::setfill('0') << metadata.sequence
				<< " delta= " << (metadata.timestamp-previous_timestamp)/1000 << " bytesused: ";
		previous_timestamp= metadata.timestamp;

		if (1) {
			// Print some information about the buffer which has completed.
			// std::cout << " seq: " << std::setw(4) << std::setfill('0') << metadata.sequence
			// 	<< " timestamp: " << metadata.timestamp
			// 	<< " bytesused: ";

			unsigned int nplane = 0;
			for (const FrameMetadata::Plane &plane : metadata.planes())
			{
				std::cout << plane.bytesused;
				if (++nplane < metadata.planes().size())
					std::cout << "/";
			}
			std::cout << std::endl;
		}

		// std::vector<libcamera::Span<uint8_t>> LibcameraApp::Mmap(FrameBuffer *buffer) const
		// {
		// 	auto item = mapped_buffers_.find(buffer);
		// 	if (item == mapped_buffers_.end())
		// 		return {};
		// 	return item->second;
		// }

		// libcamera::Span span = Mmap(buffer)[0];
		// const std::vector<libcamera::Span<uint8_t>> mem = app.Mmap(completed_request->buffers[stream]);

		// auto item = mapped_buffers_.find(request->buffers);
		// if (item == mapped_buffers_.end())
		// {
		// 	LOG(2, "mammped_buffer not found");
		// 	exit(EXIT_FAILURE);
		// }
		// const std::vector<libcamera::Span<uint8_t>> mem = item->second;

		// const std::vector<libcamera::Span<uint8_t>> spanner = mapped_buffers_[0];
		// uint8_t * mem= (uint8_t *)(mapped_buffers_[0].data());

		// #define OV9281_WIDTH 1280
		// #define OV9281_HEIGHT 800
		uint8_t * ptr= NULL;
		for (auto i= 0; i < NUMBER_OF_BUFFERS; i++)
		{
			if (mapped_buffer_ptr[i] != NULL)
			{
				ptr= mapped_buffer_ptr[i];
				LOG(4, "using buffer #" << unsigned(i) << " address="	<< std::hex << size_t(ptr) << std::dec);
				break;
			}
		}

		if (ptr != NULL)
		{
			uint32_t intensity_sum= 0;
			uint16_t intensity_average= 0;
			for (auto i=0; i < (cam_frame[WIDTH]*cam_frame[HEIGHT]); i++)
			{
				// intensity_sum += *mapped_buffer_ptr[2] + i;
				intensity_sum += *ptr++;
			}
			intensity_average= intensity_sum/(cam_frame[WIDTH]*cam_frame[HEIGHT]);
			if (intensity_average > 0)
			{
				LOG(4, "Average intensity=" << intensity_average << "Saving file.");
				char save_file_path[64];
				sprintf(save_file_path, "/run/shm/test.dat"); //, filename, cam_hostname);
				FILE * file_ptr = fopen(save_file_path,"wb");
				if (file_ptr == NULL)
					LOG(2,"Error on fileopen");
				fwrite(mapped_buffer_ptr[2], (cam_frame[WIDTH]*cam_frame[HEIGHT]), 1, file_ptr);
				fclose(file_ptr);
			}
		}

		/* -
		 * Image data can be accessed here, but the FrameBuffer must be mapped by the application
		 */

		/* TOM:
			? can the stream be retrieved from the buffer (comment out line)
			? copy png_save from apps?
			? 
		*/
		// StreamInfo info = GetStreamInfo(stream);
		// BufferReadSync r(&app, payload->buffers[stream]);
		// const std::vector<libcamera::Span<uint8_t>> mem = NULL; // r.Get();
		// png_save(mem, info, "test.png", NULL); //NULL used to be options, which were set in the app

	}

	/* Re-queue the Request to the camera. */
	request->reuse(Request::ReuseBuffers);
	camera->queueRequest(request);
}

/*
 * --------------------------------------------------------------------
 * Handle RequestComplete
 *
 * For each Camera::requestCompleted Signal emitted from the Camera the
 * connected Slot is invoked.
 *
 * The Slot is invoked in the CameraManager's thread, hence one should avoid
 * any heavy processing here. The processing of the request shall be re-directed
 * to the application's thread instead, so as not to block the CameraManager's
 * thread for large amount of time.
 *
 * The Slot receives the Request as a parameter.
 */
static void requestComplete(Request *request)
{
	if (request->status() == Request::RequestCancelled)
		return;

	loop.callLater(std::bind(&processRequest, request));
}

/*
 * ----------------------------------------------------------------------------
 * Camera Naming.
 *
 * Applications are responsible for deciding how to name cameras, and present
 * that information to the users. Every camera has a unique identifier, though
 * this string is not designed to be friendly for a human reader.
 *
 * To support human consumable names, libcamera provides camera properties
 * that allow an application to determine a naming scheme based on its needs.
 *
 * In this example, we focus on the location property, but also detail the
 * model string for external cameras, as this is more likely to be visible
 * information to the user of an externally connected device.
 *
 * The unique camera ID is appended for informative purposes.
 */
std::string cameraName(Camera *camera)
{
	const ControlList &props = camera->properties();
	std::string name;

	const auto &location = props.get(properties::Location);
	if (location) {
		switch (*location) {
		case properties::CameraLocationFront:
			name = "Internal front camera";
			break;
		case properties::CameraLocationBack:
			name = "Internal back camera";
			break;
		case properties::CameraLocationExternal:
			name = "External camera";
			const auto &model = props.get(properties::Model);
			if (model)
				name = " '" + *model + "'";
			break;
		}
	}

	name += " (" + camera->id() + ")";

	return name;
}

int main()
{
	libcamera::logSetLevel("RPI", "DEBUG");
	libcamera::logSetLevel("Camera", "DEBUG");

	/*
	 * --------------------------------------------------------------------
	 * Create a Camera Manager.
	 *
	 * The Camera Manager is responsible for enumerating all the Camera
	 * in the system, by associating Pipeline Handlers with media entities
	 * registered in the system.
	 *
	 * The CameraManager provides a list of available Cameras that
	 * applications can operate on.
	 *
	 * When the CameraManager is no longer to be used, it should be deleted.
	 * We use a unique_ptr here to manage the lifetime automatically during
	 * the scope of this function.
	 *
	 * There can only be a single CameraManager constructed within any
	 * process space.
	 */
	std::unique_ptr<CameraManager> cm = std::make_unique<CameraManager>();
	cm->start();

	/*
	 * Just as a test, generate names of the Cameras registered in the
	 * system, and list them.
	 */
	for (auto const &camera : cm->cameras())
		std::cout << "Camera Name: " << cameraName(camera.get()) << std::endl;

	/*
	 * --------------------------------------------------------------------
	 * Camera
	 *
	 * Camera are entities created by pipeline handlers, inspecting the
	 * entities registered in the system and reported to applications
	 * by the CameraManager.
	 *
	 * In general terms, a Camera corresponds to a single image source
	 * available in the system, such as an image sensor.
	 *
	 * Application lock usage of Camera by 'acquiring' them.
	 * Once done with it, application shall similarly 'release' the Camera.
	 *
	 * As an example, use the first available camera in the system after
	 * making sure that at least one camera is available.
	 *
	 * Cameras can be obtained by their ID or their index, to demonstrate
	 * this, the following code gets the ID of the first camera; then gets
	 * the camera associated with that ID (which is of course the same as
	 * cm->cameras()[0]).
	 */
	if (cm->cameras().empty()) {
		std::cout << "No cameras were identified on the system."
			  << std::endl;
		cm->stop();
		return EXIT_FAILURE;
	}

	std::string cameraId = cm->cameras()[0]->id();
	camera = cm->get(cameraId);
	camera->acquire();

	/*
	 * Stream
	 *
	 * Each Camera supports a variable number of Stream. A Stream is
	 * produced by processing data produced by an image source, usually
	 * by an ISP.
	 *
	 *   +-------------------------------------------------------+
	 *   | Camera                                                |
	 *   |                +-----------+                          |
	 *   | +--------+     |           |------> [  Main output  ] |
	 *   | | Image  |     |           |                          |
	 *   | |        |---->|    ISP    |------> [   Viewfinder  ] |
	 *   | | Source |     |           |                          |
	 *   | +--------+     |           |------> [ Still Capture ] |
	 *   |                +-----------+                          |
	 *   +-------------------------------------------------------+
	 *
	 * The number and capabilities of the Stream in a Camera are
	 * a platform dependent property, and it's the pipeline handler
	 * implementation that has the responsibility of correctly
	 * report them.
	 */

	/*
	 * --------------------------------------------------------------------
	 * Camera Configuration.
	 *
	 * Camera configuration is tricky! It boils down to assign resources
	 * of the system (such as DMA engines, scalers, format converters) to
	 * the different image streams an application has requested.
	 *
	 * Depending on the system characteristics, some combinations of
	 * sizes, formats and stream usages might or might not be possible.
	 *
	 * A Camera produces a CameraConfigration based on a set of intended
	 * roles for each Stream the application requires.
	 */
	std::unique_ptr<CameraConfiguration> config =
		camera->generateConfiguration( { StreamRole::StillCapture} ); //::Raw instead of ::Viewfinder; Raw didn't do a capture

	/*
	 * The CameraConfiguration contains a StreamConfiguration instance
	 * for each StreamRole requested by the application, provided
	 * the Camera can support all of them.
	 *
	 * Each StreamConfiguration has default size and format, assigned
	 * by the Camera depending on the Role the application has requested.
	 */
	StreamConfiguration &streamConfig = config->at(0);
	std::cout << "Default Stream config: "
		  << streamConfig.toString() << std::endl;

	/*
	 * Each StreamConfiguration parameter which is part of a
	 * CameraConfiguration can be independently modified by the
	 * application.
	 *
	 * In order to validate the modified parameter, the CameraConfiguration
	 * should be validated -before- the CameraConfiguration gets applied
	 * to the Camera.
	 *
	 * The CameraConfiguration validation process adjusts each
	 * StreamConfiguration to a valid value.
	 */

	// refer to: https://www.libcamera.org/api-html/build_2include_2libcamera_2formats_8h_source.html
	// const char pixFormat[] = "R8  "; //This didn't work, nor GREY nor YU16.  Must be something with OV9281 supporting code
	if (0)
	{
		const char pixFormat[] = "GREY";
		int rc2 = streamConfig.pixelFormat.fromString(pixFormat);
		std::cout << "pixelFormat.fromString return=" << rc2 << std::endl;
	}
	streamConfig.bufferCount= NUMBER_OF_BUFFERS;	

	/*
	 * Validating a CameraConfiguration -before- applying it will adjust it
	 * to a valid configuration which is as close as possible to the one
	 * requested.
	 */
	config->validate();
	std::cout << "Validated Stream Config: "
		  << streamConfig.toString() << std::endl;

	// LOG(4, "width=" << streamConfig.size.width);
	cam_frame[WIDTH]= streamConfig.size.width;
	cam_frame[HEIGHT]= streamConfig.size.height;

	camera->configure(config.get());

	/*
	 * --------------------------------------------------------------------
	 * Buffer Allocation
	 *
	 * Now that a camera has been configured, it knows all about its
	 * Streams sizes and formats. The captured images need to be stored in
	 * framebuffers which can either be provided by the application to the
	 * library, or allocated in the Camera and exposed to the application
	 * by libcamera.
	 *
	 * An application may decide to allocate framebuffers from elsewhere,
	 * for example in memory allocated by the display driver that will
	 * render the captured frames. The application will provide them to
	 * libcamera by constructing FrameBuffer instances to capture images
	 * directly into.
	 *
	 * Alternatively libcamera can help the application by exporting
	 * buffers allocated in the Camera using a FrameBufferAllocator
	 * instance and referencing a configured Camera to determine the
	 * appropriate buffer size and types to create.
	 */
	FrameBufferAllocator *allocator = new FrameBufferAllocator(camera);

	for (StreamConfiguration &cfg : *config) {
		int ret = allocator->allocate(cfg.stream());
		if (ret < 0) {
			std::cerr << "Can't allocate buffers" << std::endl;
			return EXIT_FAILURE;
		}

		size_t allocated = allocator->buffers(cfg.stream()).size();
		std::cout << "Allocated " << allocated << " buffers for stream" << std::endl;
	}

	// Stream *stream = streamConfig.stream();  //stream was made a global so buffers could be accessed
	stream = streamConfig.stream();
	const std::vector<std::unique_ptr<FrameBuffer>> &buffers = allocator->buffers(stream);
	
	if (1)
	{
		unsigned buffer_number= 0;
		for (const std::unique_ptr<FrameBuffer> &buffer : allocator->buffers(stream)) //iterate through buffers
		{
			// "Single plane" buffers appear as multi-plane here, but we can spot them because then
			// planes all share the same fd. We accumulate them so as to mmap the buffer only once.
			// this logs there are 3 planes per buffer: 		std::clog << "planes=" << buffer->planes().size() << std::endl;
			size_t buffer_size = 0;
			for (unsigned i = 0; i < buffer->planes().size(); i++)
			{
				const FrameBuffer::Plane &plane = buffer->planes()[i];
				buffer_size += plane.length;
				if (i == buffer->planes().size() - 1 || plane.fd.get() != buffer->planes()[i + 1].fd.get())
				{
					void *memory = mmap(NULL, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, plane.fd.get(), 0);
					LOG(5, "buffer mapping iteration=" << buffer_number << " memory=" << std::hex << size_t(memory) << std::dec);
					mapped_buffer_ptr[i]= (uint8_t *) memory;

					mapped_buffers_[buffer.get()].push_back(
						libcamera::Span<uint8_t>(static_cast<uint8_t *>(memory), buffer_size));
					buffer_size = 0;
				}
			}
			frame_buffers_[stream].push(buffer.get());
			buffer_number++;
		}
	}

	/*
	 * --------------------------------------------------------------------
	 * Frame Capture
	 *
	 * libcamera frames capture model is based on the 'Request' concept.
	 * For each frame a Request has to be queued to the Camera.
	 *
	 * A Request refers to (at least one) Stream for which a Buffer that
	 * will be filled with image data shall be added to the Request.
	 *
	 * A Request is associated with a list of Controls, which are tunable
	 * parameters (similar to v4l2_controls) that have to be applied to
	 * the image.
	 *
	 * Once a request completes, all its buffers will contain image data
	 * that applications can access and for each of them a list of metadata
	 * properties that reports the capture parameters applied to the image.
	 */
	std::vector<std::unique_ptr<Request>> requests;
	//create a request per buffer
	for (unsigned int i = 0; i < buffers.size(); ++i) {
		std::unique_ptr<Request> request = camera->createRequest();
		if (!request)
		{
			std::cerr << "Can't create request" << std::endl;
			return EXIT_FAILURE;
		}

		const std::unique_ptr<FrameBuffer> &buffer = buffers[i];
		int ret = request->addBuffer(stream, buffer.get());
		if (ret < 0)
		{
			std::cerr << "Can't set buffer for request"
				  << std::endl;
			return EXIT_FAILURE;
		}

		//Controls can be added to a request on a per frame basis.
		ControlList &controls = request->controls();
		// the following doesn't work (the controls list is empty?
		for (const auto &ctrl : controls) {
			const ControlId *id = controls::controls.at(ctrl.first);
			const ControlValue &value = ctrl.second;
			std::cout << "\t" << id->name() << " = " << value.toString()
				<< std::endl;
		}

		// controls.set(controls::ExposureCustom, true); //this causes an assert
		if (i & 0x1)
		{
			controls.set(controls::ExposureTime, 9000); //microseconds
			// std::cout << "Buffer " << i << " expos=9000" << std::endl;
		} else
		{
			controls.set(controls::ExposureTime, 20000);
			// std::cout << "Buffer " << i << " expos=20000" << std::endl;
		}
		// set the frame spacing
		std::int64_t value_pair[2] = {119000, 121000}; //lower, upper bounds in microseconds
		controls.set(libcamera::controls::FrameDurationLimits, libcamera::Span<const std::int64_t, 2>(value_pair));

		// controls.set(controls::Brightness, 0.5);
		controls.set(controls::AeEnable, false);  // Auto exposure
		controls.set(controls::AnalogueGain, 90);
		controls.set(controls::AeFlickerMode, 0); //off
		// controls.set(controls::AwbEnable, false); // auto ? NOT valid for ov9281

		requests.push_back(std::move(request));
	}

	/*
	 * --------------------------------------------------------------------
	 * Signal&Slots
	 *
	 * libcamera uses a Signal&Slot based system to connect events to
	 * callback operations meant to handle them, inspired by the QT graphic
	 * toolkit.
	 *
	 * Signals are events 'emitted' by a class instance.
	 * Slots are callbacks that can be 'connected' to a Signal.
	 *
	 * A Camera exposes Signals, to report the completion of a Request and
	 * the completion of a Buffer part of a Request to support partial
	 * Request completions.
	 *
	 * In order to receive the notification for request completions,
	 * applications shall connecte a Slot to the Camera 'requestCompleted'
	 * Signal before the camera is started.
	 */
	camera->requestCompleted.connect(requestComplete);

	/*
	 * --------------------------------------------------------------------
	 * Start Capture
	 *
	 * In order to capture frames the Camera has to be started and
	 * Request queued to it. Enough Request to fill the Camera pipeline
	 * depth have to be queued before the Camera start delivering frames.
	 *
	 * For each delivered frame, the Slot connected to the
	 * Camera::requestCompleted Signal is called.
	 */
	camera->start();
	for (std::unique_ptr<Request> &request : requests)
		camera->queueRequest(request.get());

	/*
	 * --------------------------------------------------------------------
	 * Run an EventLoop
	 *
	 * In order to dispatch events received from the video devices, such
	 * as buffer completions, an event loop has to be run.
	 */
	loop.timeout(TIMEOUT_SEC);
	int ret = loop.exec();
	std::cout << std::endl << "Capture ran for " << TIMEOUT_SEC << " seconds and "
		  << "stopped with exit status: " << ret << std::endl;

	/*
	 * --------------------------------------------------------------------
	 * Clean Up
	 *
	 * Stop the Camera, release resources and stop the CameraManager.
	 * libcamera has now released all resources it owned.
	 */
	camera->stop();
	allocator->free(stream);
	delete allocator;
	camera->release();
	camera.reset();
	cm->stop();

	return EXIT_SUCCESS;
}
