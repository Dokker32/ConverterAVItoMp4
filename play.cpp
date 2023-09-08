#include <iostream>
#include <csignal>
#include <gst/gst.h>


static GMainLoop * main_loop;
//Обработка сигнала SIGINT
static void sigint_handler(int sig) {
  std::cout << "Получен сигнал SIGINT, Выход" << std::endl;
  g_main_loop_quit(main_loop);
}
// Обработка сообщений из шины (пайплайна)
static gboolean bus_callback(GstBus * bus, GstMessage * message, gpointer data) {
  switch (GST_MESSAGE_TYPE(message)) {
  case GST_MESSAGE_ERROR: {
    GError * err;
    gchar * debug_info;
    gst_message_parse_error(message, & err, & debug_info);
    std::cerr << "Ошибка элемента " << GST_OBJECT_NAME(message -> src) <<
      ": " << err -> message << std::endl;
    std::cerr << "Debugg: " << (debug_info ? debug_info : "none") << std::endl;
    g_clear_error( & err);
    g_free(debug_info);
    g_main_loop_quit(main_loop);
    break;
  }
  case GST_MESSAGE_EOS:
    std::cout << "Конец стрима" << std::endl;
    g_main_loop_quit(main_loop);
    break;
  default:
    break;
  }
  return TRUE;
}
// Обработчик события динамического добавления потоков
static void on_pad_added(GstElement * element, GstPad * pad, gpointer data) {
  GstCaps * caps = gst_pad_get_current_caps(pad);
  GstStructure * capsStructure = gst_caps_get_structure(caps, 0);
  const gchar * name = gst_structure_get_name(capsStructure);
  gchar * padName = gst_pad_get_name(pad);
  std::cout << "Pad: " << padName << " " << name << std::endl;

  GstElement * convertElement = nullptr;

  if (g_str_has_prefix(name, "audio")) {
    convertElement = (GstElement * ) data;
  } else if (g_str_has_prefix(name, "video")) {
    convertElement = (GstElement * )((void ** ) data)[1];
  }

  if (convertElement) {
    GstPad * convertPad = gst_element_get_static_pad(convertElement, "sink");
    GstPadLinkReturn linkResult = gst_pad_link(pad, convertPad);
    std::cout << "Линковка:" << padName << " -> " << gst_element_get_name(convertElement) <<
      " статус: " << gst_pad_link_get_name(linkResult) << std::endl;
    gst_object_unref(convertPad);
  }

  g_free(padName);
  gst_caps_unref(caps);
}

int main(int argc, char * argv[]) {
  // Проверка аргументов
  if (argc != 3) {
    std::cerr << "Пример использовапния " << argv[0] << " in.avi out.mp4" << std::endl;
    return 1;
  }

  // Инциализация GStreamer
  gst_init( & argc, & argv);

  // Создаем конвейер
  GstElement * pipeline, * source, * decodebin, * video_convert, * x264enc, * h264parse, * mp4mux, * filesink,
    * audio_convert, * aacenc;

  pipeline = gst_pipeline_new("pipeline");
  source = gst_element_factory_make("filesrc", "source");
  decodebin = gst_element_factory_make("decodebin", "decodebin");
  video_convert = gst_element_factory_make("videoconvert", "video_convert");
  x264enc = gst_element_factory_make("x264enc", "x264enc");
  h264parse = gst_element_factory_make("h264parse", "h264parse");
  audio_convert = gst_element_factory_make("audioconvert", "audio_convert");
  aacenc = gst_element_factory_make("avenc_aac", "aacenc");
  mp4mux = gst_element_factory_make("mp4mux", "mp4mux");
  filesink = gst_element_factory_make("filesink", "filesink");
  GstElement * h264_decoder = gst_element_factory_make("avdec_h264", "h264-decoder");

  if (!pipeline || !source || !decodebin || !video_convert || !x264enc || !h264parse || !audio_convert || !aacenc ||
    !mp4mux || !filesink) {
    std::cerr << "Один или несколько элементов не могут быть созданы, Выход" << std::endl;
    return 1;
  }

  // Задаем вход и выход
  g_object_set(G_OBJECT(source), "location", argv[1], NULL);
  g_object_set(G_OBJECT(filesink), "location", argv[2], NULL);

  // Добавляем элементы в конвейер
  gst_bin_add_many(GST_BIN(pipeline), source, h264_decoder, decodebin, video_convert, x264enc, h264parse, audio_convert, aacenc, mp4mux, filesink, NULL);

  // Соединяем элементы
  gst_element_link(source, decodebin);
  gst_element_link_many(video_convert, x264enc, h264parse, NULL);
  gst_element_link_many(audio_convert, aacenc, NULL);
  gst_element_link_many(h264parse, mp4mux, filesink, NULL);
  gst_element_link_many(aacenc, mp4mux, NULL);

  // Подключение к сигналу pad-added для динамически созданных источников
  g_signal_connect(decodebin, "pad-added", G_CALLBACK(on_pad_added), (void * ) audio_convert);

  gpointer data[] = {
    audio_convert,
    video_convert
  };
  g_signal_connect(decodebin, "pad-added", G_CALLBACK(on_pad_added), (void * ) data);

  // Шина передачи между элементами
  GstBus * bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
  gst_bus_add_watch(bus, bus_callback, nullptr);

  // Запуск конвейера
  gst_element_set_state(pipeline, GST_STATE_PLAYING);

  // Обработчик SIGINT
  signal(SIGINT, sigint_handler);

  // Цикл приложения
  main_loop = g_main_loop_new(nullptr, FALSE);
  g_main_loop_run(main_loop);

  // Чистим память
  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(pipeline);
  gst_object_unref(bus);
  g_main_loop_unref(main_loop);

  return 0;
}