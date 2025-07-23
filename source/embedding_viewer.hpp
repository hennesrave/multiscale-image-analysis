#pragma once
#include "database.hpp"
#include "utility.hpp"

#include <qoffscreensurface.h>
#include <qopenglbuffer.h>
#include <qopenglcontext.h>
#include <qopenglframebufferobject.h>
#include <qopenglfunctions_4_5_core.h>
#include <qopenglshaderprogram.h>
#include <qopenglvertexarrayobject.h>

#include <qsharedpointer.h>
#include <qwidget.h>

struct RENDERDOC_API_1_6_0;

// ----- EmbeddingRenderer ----- //

class EmbeddingRenderer : public QObject
{
public:
	static constexpr uint32_t default_texture_size = 1024;

	EmbeddingRenderer();

	int texture_size() const noexcept;
	void update_texture_size( int texture_size );

	void update_points( const std::vector<vec2<float>>& point_positions, const std::vector<uint32_t>& point_indices );
	void update_segmentation( Array<uint32_t> segmentation_numbers, Array<vec4<float>> segment_colors );

	QImage render( const QMatrix4x4& projection_matrix, GLfloat point_size );
	Array<uint32_t> update_segmentation( const QMatrix4x4& projection_matrix, const Array<vec2<float>>& selection_polygon, uint32_t segment_number );

private:
	QOpenGLContext _context;
	QOffscreenSurface _surface;
	QOpenGLFunctions_4_5_Core* _functions = nullptr;
	RENDERDOC_API_1_6_0* _renderdoc = nullptr;

	QOpenGLVertexArrayObject _vertex_array_object;

	int _texture_size = EmbeddingRenderer::default_texture_size;
	std::unique_ptr<QOpenGLFramebufferObject> _framebuffer;

	struct
	{
		QOpenGLBuffer point_positions;
		QOpenGLBuffer point_indices;
		QOpenGLBuffer segmentation_numbers;
		QOpenGLBuffer segment_colors;
		QOpenGLBuffer selection_polygon;
	} _buffers;

	struct
	{
		QOpenGLShaderProgram render_points;
		QOpenGLShaderProgram postprocessing;
		QOpenGLShaderProgram render_selection_polygon;
		QOpenGLShaderProgram update_segmentation;
	} _shader_programs;

	GLint _maximum_compute_work_group_size = 0;
};

// ----- EmbeddingViewer ----- //

class EmbeddingViewer : public QWidget
{
	Q_OBJECT
public:
	enum class SelectionMode
	{
		eNone,
		eGrowSegment,
		eShrinkSegment,
	};

	class SearchTree
	{
	public:
		struct Partition
		{
			uint32_t begin { 0 };
			uint32_t end { 0 };

			enum Axis { eNone, eX, eY } axis { eNone };
			float value { 0.0f };
			uint32_t lower { 0 };
			uint32_t upper { 0 };
		};

		SearchTree( const std::vector<vec2<float>>& points );

		const std::vector<vec2<float>>& points() const noexcept;
		uint32_t nearest_neighbor( vec2<float> point );

	private:
		uint32_t construct_partitions_recursive( Partition::Axis axis, uint32_t begin, uint32_t end );

		const std::vector<vec2<float>>& _points;
		std::vector<uint32_t> _indices;
		std::vector<Partition> _partitions;
	};

	EmbeddingViewer( Database& database );

	void resizeEvent( QResizeEvent* event ) override;
	void paintEvent( QPaintEvent* event ) override;
	void wheelEvent( QWheelEvent* event ) override;

	void mousePressEvent( QMouseEvent* event ) override;
	void mouseReleaseEvent( QMouseEvent* event ) override;

	void mouseMoveEvent( QMouseEvent* event ) override;
	void leaveEvent( QEvent* event ) override;

	double world_to_screen_x( double value ) const;
	double world_to_screen_y( double value ) const;
	QPointF world_to_screen( const QPointF& world ) const;

	double screen_to_world_x( double value ) const;
	double screen_to_world_y( double value ) const;
	QPointF screen_to_world( const QPointF& screen ) const;

private:
	void segmentation_changed();
	void reset_projection_matrix();
	void import_embedding( const QString& filepath );
	void create_screenshot( uint32_t scaling ) const;

	Database& _database;
	std::unique_ptr<EmbeddingRenderer> _renderer;
	bool _scatterplot_image_valid = false;
	QImage _scatterplot_image;

	std::vector<vec2<float>> _point_positions;
	std::vector<uint32_t> _point_indices;
	std::unique_ptr<SearchTree> _search_tree;

	QMatrix4x4 _projection_matrix;
	GLfloat _point_size = 1.0f;

	QPointF _cursor_position { -1.0, -1.0 };
	QPolygonF _selection_polygon;
	SelectionMode _selection_mode = SelectionMode::eNone;
};