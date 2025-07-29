#include "embedding_viewer.hpp"

#include "embedding_creator.hpp"
#include "renderdoc_app.h"
#include "segmentation_manager.hpp"

#include <qactiongroup.h>
#include <qapplication.h>
#include <qbuffer.h>
#include <qclipboard.h>
#include <qevent.h>
#include <qfiledialog.h>
#include <qlabel.h>
#include <qmessagebox.h>
#include <qmenu.h>
#include <qmimedata.h>
#include <qopenglversionfunctionsfactory.h>
#include <qpainter.h>
#include <qwidgetaction.h>

#define NOMINMAX
#include <windows.h>

// ----- EmbeddingRenderer ----- //

EmbeddingRenderer::EmbeddingRenderer()
{
    auto surface_format = QSurfaceFormat {};
    surface_format.setProfile( QSurfaceFormat::CoreProfile );
    surface_format.setVersion( 4, 5 );

    _context.setFormat( surface_format );
    _context.create();

    _surface.setFormat( surface_format );
    _surface.create();

    _context.makeCurrent( &_surface );
    _functions = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_4_5_Core>( &_context );
    _functions->initializeOpenGLFunctions();

    if( const auto module = GetModuleHandleA( "renderdoc.dll" ) )
    {
        const auto RENDERDOC_GetAPI = (pRENDERDOC_GetAPI) GetProcAddress( module, "RENDERDOC_GetAPI" );
        RENDERDOC_GetAPI( eRENDERDOC_API_Version_1_1_2, (void**) &_renderdoc );
    }

    _vertex_array_object.create();
    _vertex_array_object.bind();

    auto framebuffer_format = QOpenGLFramebufferObjectFormat {};
    framebuffer_format.setAttachment( QOpenGLFramebufferObject::NoAttachment );
    framebuffer_format.setInternalTextureFormat( GL_RGBA32F );
    _framebuffer.reset( new QOpenGLFramebufferObject( QSize { _texture_size, _texture_size }, framebuffer_format ) );

    _shader_programs.render_points.create();
    _shader_programs.render_points.addShaderFromSourceCode( QOpenGLShader::Vertex, R"(
        #version 450
        
        layout( location = 0 ) uniform mat4 projection_matrix;
        layout( location = 1 ) uniform float point_size;

        layout( binding = 0 ) restrict readonly buffer PointPositionsBuffer
        {
            vec2 point_positions[];
        };
        layout( std430, binding = 1 ) restrict readonly buffer PointIndicesBuffer
        {
            uint point_indices[];
        };
        layout( std430, binding = 2 ) restrict readonly buffer SegmentationNumbersBuffer
        {
            uint segmentation_numbers[];
        };
        layout( binding = 3 ) restrict readonly buffer SegmentationColorsBuffer
        {
            vec4 segmentation_colors[];
        };

        layout( location = 0 ) out vec4 color_output;

        void main()
        {
            gl_Position     = projection_matrix * vec4( point_positions[gl_VertexID], 0.0, 1.0 );
            gl_PointSize    = point_size;

            const uint point_index      = point_indices[gl_VertexID];
            const uint segment_number   = segmentation_numbers[point_index];
            color_output                = segmentation_colors[segment_number];
        }
    )" );
    _shader_programs.render_points.addShaderFromSourceCode( QOpenGLShader::Fragment, R"(
        #version 450
        
        layout( location = 0 ) in vec4 color_input;
        layout( location = 0 ) out vec4 color_output;

        void main()
        {
            const vec2 coordinates = gl_PointCoord - vec2( 0.5 );
            const float distance = length( coordinates );
            if( distance > 0.5 )
                discard;

            color_output = vec4( 1.0 - color_input.rgb, 1.0 );
        }
    )" );
    _shader_programs.render_points.link();

    _shader_programs.postprocessing.create();
    _shader_programs.postprocessing.addShaderFromSourceCode( QOpenGLShader::Compute, R"(
        #version 450

        layout(local_size_x = 16, local_size_y = 16) in;

        layout(rgba32f) uniform image2D image;

        void main() {
            const ivec2 coordinates     = ivec2( gl_GlobalInvocationID.xy );
            const vec4 color_input      = imageLoad( image, coordinates );
            const vec4 color_output     = vec4( 1.0 - color_input.rgb / ( 1.0 + color_input.a ), color_input.a );
            imageStore( image, coordinates, color_output );
        }
    )" );
    _shader_programs.postprocessing.link();

    _shader_programs.render_selection_polygon.create();
    _shader_programs.render_selection_polygon.addShaderFromSourceCode( QOpenGLShader::Vertex, R"(
        #version 450
        
        layout( location = 0 ) uniform mat4 projection_matrix;
        layout( binding = 0 ) restrict readonly buffer PointPositionsBuffer
        {
            vec2 point_positions[];
        };

        void main()
        {
            gl_Position = projection_matrix * vec4( point_positions[gl_VertexID], 0.0, 1.0 );
        }
    )" );
    _shader_programs.render_selection_polygon.addShaderFromSourceCode( QOpenGLShader::Fragment, R"(
        #version 450

        layout( location = 0 ) out vec4 color_output;

        void main()
        {
            color_output = vec4( 1.0 );
        }
    )" );
    _shader_programs.render_selection_polygon.link();

    auto update_segmentation_shader_code = std::string { R"(
        #version 450

        layout( local_size_x = WORK_GROUP_SIZE_X ) in;
        layout( location = 0 ) uniform mat4 projection_matrix;
        layout( location = 1 ) uniform uint point_count;
        layout( location = 2 ) uniform uint segment_number;
        layout( binding = 0 ) restrict readonly buffer PointPositionsBuffer
        {
            vec2 point_positions[];
        };
        layout( std430, binding = 1 ) restrict readonly buffer PointIndicesBuffer
        {
            uint point_indices[];
        };
        layout( std430, binding = 2 ) restrict buffer SegmentationNumbersBuffer
        {
            uint segmentation_numbers[];
        };
        layout( binding = 3 ) uniform sampler2D selection_texture;

        void main()
        {
            const uint index = uint( gl_GlobalInvocationID );
            if( index < point_count )
            {
                const vec2 position = ( projection_matrix * vec4( point_positions[index], 0.0, 1.0 ) ).xy;
                const vec2 texture_position = ( position + 1.0 ) / 2.0;
                if( texture( selection_texture, texture_position ).x != 0.0 )
                {
                    segmentation_numbers[point_indices[index]] = segment_number;
                }
            }
        }
    )" };
    const auto token = std::string { "WORK_GROUP_SIZE_X" };
    _functions->glGetIntegeri_v( GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &_maximum_compute_work_group_size );
    update_segmentation_shader_code.replace( update_segmentation_shader_code.find( token ), token.length(), std::to_string( _maximum_compute_work_group_size ) );

    _shader_programs.update_segmentation.create();
    _shader_programs.update_segmentation.addShaderFromSourceCode( QOpenGLShader::Compute, update_segmentation_shader_code.data() );
    _shader_programs.update_segmentation.link();

    _buffers.point_positions.create();
    _buffers.point_indices.create();
    _buffers.segmentation_numbers.create();
    _buffers.segment_colors.create();
    _buffers.selection_polygon.create();
}

int EmbeddingRenderer::texture_size() const noexcept
{
    return _texture_size;
}
void EmbeddingRenderer::update_texture_size( int texture_size )
{
    _texture_size = texture_size;

    _context.makeCurrent( &_surface );
    auto framebuffer_format = QOpenGLFramebufferObjectFormat {};
    framebuffer_format.setAttachment( QOpenGLFramebufferObject::NoAttachment );
    framebuffer_format.setInternalTextureFormat( GL_RGBA32F );
    _framebuffer.reset( new QOpenGLFramebufferObject( QSize { _texture_size, _texture_size }, framebuffer_format ) );
    _context.doneCurrent();
}
void EmbeddingRenderer::update_points( const std::vector<vec2<float>>& point_positions, const std::vector<uint32_t>& point_indices )
{
    _context.makeCurrent( &_surface );

    _buffers.point_positions.bind();
    _buffers.point_positions.allocate( point_positions.data(), static_cast<int>( point_positions.size() * sizeof( vec2<float> ) ) );

    _buffers.point_indices.bind();
    _buffers.point_indices.allocate( point_indices.data(), static_cast<int>( point_indices.size() * sizeof( uint32_t ) ) );

    _context.doneCurrent();
}
void EmbeddingRenderer::update_segmentation( Array<uint32_t> segmentation_numbers, Array<vec4<float>> segment_colors )
{
    _context.makeCurrent( &_surface );

    _buffers.segmentation_numbers.bind();
    _buffers.segmentation_numbers.allocate( segmentation_numbers.data(), static_cast<int>( segmentation_numbers.size() * sizeof( uint32_t ) ) );

    _buffers.segment_colors.bind();
    _buffers.segment_colors.allocate( segment_colors.data(), static_cast<int>( segment_colors.size() * sizeof( vec4<float> ) ) );

    _context.doneCurrent();
}

QImage EmbeddingRenderer::render( const QMatrix4x4& projection_matrix, GLfloat point_size )
{
    _context.makeCurrent( &_surface );

    _buffers.point_positions.bind();
    const auto point_count = static_cast<GLsizei>( _buffers.point_positions.size() / sizeof( vec2<float> ) );
    if( !point_count )
    {
        return QImage {};
    }

    // if( _renderdoc ) _renderdoc->StartFrameCapture( nullptr, nullptr );

    _vertex_array_object.bind();

    _framebuffer->bind();
    _functions->glViewport( 0, 0, _texture_size, _texture_size );
    _functions->glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
    _functions->glClear( GL_COLOR_BUFFER_BIT );

    _shader_programs.render_points.bind();
    _shader_programs.render_points.setUniformValue( "projection_matrix", projection_matrix );
    _shader_programs.render_points.setUniformValue( "point_size", point_size );

    _functions->glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, _buffers.point_positions.bufferId() );
    _functions->glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 1, _buffers.point_indices.bufferId() );
    _functions->glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 2, _buffers.segmentation_numbers.bufferId() );
    _functions->glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 3, _buffers.segment_colors.bufferId() );

    _functions->glEnable( GL_PROGRAM_POINT_SIZE );
    _functions->glEnable( GL_BLEND );
    _functions->glBlendFunc( GL_ONE, GL_ONE );

    _functions->glDrawArrays( GL_POINTS, 0, point_count );

    _shader_programs.postprocessing.bind();
    _functions->glBindImageTexture( 0, _framebuffer->texture(), 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F );
    _functions->glDispatchCompute( _texture_size / 16, _texture_size / 16, 1 );
    _functions->glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );

    // if( _renderdoc ) _renderdoc->EndFrameCapture( nullptr, nullptr );

    const auto image = _framebuffer->toImage();

    _framebuffer->release();
    _context.doneCurrent();

    return image;
}
Array<uint32_t> EmbeddingRenderer::update_segmentation( const QMatrix4x4& projection_matrix, const Array<vec2<float>>& selection_polygon, uint32_t segment_number )
{
    _context.makeCurrent( &_surface );

    if( _renderdoc )
    {
        _renderdoc->StartFrameCapture( nullptr, nullptr );
    }

    _buffers.segmentation_numbers.bind();
    const auto element_count = static_cast<uint32_t>( _buffers.segmentation_numbers.size() / sizeof( uint32_t ) );

    _buffers.point_positions.bind();
    const auto point_count = static_cast<GLsizei>( _buffers.point_positions.size() / sizeof( vec2<float> ) );

    _buffers.selection_polygon.bind();
    _buffers.selection_polygon.allocate( selection_polygon.data(), static_cast<int>( selection_polygon.size() * sizeof( vec2<float> ) ) );

    _vertex_array_object.bind();

    _framebuffer->bind();
    _functions->glViewport( 0, 0, _texture_size, _texture_size );
    _functions->glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
    _functions->glClear( GL_COLOR_BUFFER_BIT );

    _shader_programs.render_selection_polygon.bind();
    _functions->glUniformMatrix4fv( 0, 1, false, projection_matrix.data() );
    _functions->glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, _buffers.selection_polygon.bufferId() );

    _functions->glEnable( GL_BLEND );
    _functions->glBlendFunc( GL_ONE_MINUS_DST_COLOR, GL_ZERO );

    _functions->glDrawArrays( GL_TRIANGLE_FAN, 0, static_cast<GLsizei>( selection_polygon.size() ) );

    _shader_programs.update_segmentation.bind();
    _functions->glUniformMatrix4fv( 0, 1, false, projection_matrix.data() );
    _functions->glUniform1ui( 1, point_count );
    _functions->glUniform1ui( 2, segment_number );
    _functions->glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, _buffers.point_positions.bufferId() );
    _functions->glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 1, _buffers.point_indices.bufferId() );
    _functions->glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 2, _buffers.segmentation_numbers.bufferId() );

    _functions->glActiveTexture( GL_TEXTURE3 );
    _functions->glBindTexture( GL_TEXTURE_2D, _framebuffer->texture() );

    const auto work_group_count = static_cast<int>( std::ceil( static_cast<double>( point_count ) / _maximum_compute_work_group_size ) );
    _functions->glDispatchCompute( work_group_count, 1, 1 );
    _functions->glMemoryBarrier( GL_SHADER_STORAGE_BARRIER_BIT );

    auto segmentation_numbers = Array<uint32_t>::allocate( element_count );
    _buffers.segmentation_numbers.bind();
    _buffers.segmentation_numbers.read( 0, segmentation_numbers.data(), static_cast<int>( element_count * sizeof( uint32_t ) ) );

    if( _renderdoc )
    {
        _renderdoc->EndFrameCapture( nullptr, nullptr );
    }

    _framebuffer->release();
    _context.doneCurrent();

    return segmentation_numbers;
}

// ----- EmbeddingViewer ----- //

EmbeddingViewer::EmbeddingViewer( Database& database ) : _database { database }, _renderer { new EmbeddingRenderer }
{
    this->setMouseTracking( true );

    const auto segmentation = _database.segmentation();
    QObject::connect( segmentation.get(), &Segmentation::segment_count_changed, this, &EmbeddingViewer::segmentation_changed );
    QObject::connect( segmentation.get(), &Segmentation::element_colors_changed, this, &EmbeddingViewer::segmentation_changed );
    this->segmentation_changed();

    QObject::connect( &_database, &Database::highlighted_element_index_changed, this, qOverload<>( &QWidget::update ) );
}

void EmbeddingViewer::resizeEvent( QResizeEvent* event )
{
    QWidget::resizeEvent( event );
    this->reset_projection_matrix();
}
void EmbeddingViewer::paintEvent( QPaintEvent* event )
{
    auto painter = QPainter { this };
    painter.setRenderHint( QPainter::Antialiasing );

    const auto extent = std::min( this->width(), this->height() ) - 20;
    const auto center = this->rect().center();
    auto content_rectangle = QRect {};
    content_rectangle.setSize( QSize { extent, extent } );
    content_rectangle.moveCenter( center );

    // Render scatterplot
    if( !_scatterplot_image_valid )
    {
        const auto adjusted_point_size = ( _point_size * std::sqrt( _projection_matrix( 0, 0 ) ) ) / 72.0 * QGuiApplication::primaryScreen()->physicalDotsPerInch() / ( static_cast<double>( extent ) / _renderer->texture_size() );
        _scatterplot_image = _renderer->render( _projection_matrix, adjusted_point_size );
        _scatterplot_image_valid = true;
    }

    painter.setCompositionMode( QPainter::CompositionMode_Source );
    painter.drawImage( content_rectangle, _scatterplot_image );
    painter.setCompositionMode( QPainter::CompositionMode_SourceOver );

    // Render highlighted element
    if( const auto element_index = _database.highlighted_element_index(); element_index.has_value() )
    {
        const auto iterator = std::lower_bound( _point_indices.begin(), _point_indices.end(), *element_index );
        if( iterator != _point_indices.end() && *iterator == *element_index )
        {
            const auto position = _point_positions[std::distance( _point_indices.begin(), iterator )];
            const auto screen = this->world_to_screen( QPointF { position.x, position.y } );

            const auto segment_number = _database.segmentation()->segment_number( *element_index );
            const auto segment = _database.segmentation()->segment( segment_number );
            const auto color = segment_number == 0 ? QColor { 230, 230, 230, 255 } : segment->color().qcolor();

            const auto radius = std::sqrt( _projection_matrix( 0, 0 ) ) * _point_size + 3.0;

            painter.setPen( QPen { QColor { Qt::black }, 1.0 } );
            painter.setBrush( color );
            painter.drawEllipse( screen, radius, radius );
        }
    }

    // Render border
    painter.setPen( Qt::black );
    painter.setBrush( Qt::NoBrush );
    painter.drawRect( content_rectangle );

    // Render selection polygon
    if( !_selection_polygon.empty() )
    {
        const auto stroke_color = _selection_mode == SelectionMode::eGrowSegment ? _database.active_segment()->color().qcolor() : QColor { 200, 200, 200 };
        auto brush_color = stroke_color;
        brush_color.setAlpha( 150 );

        painter.setPen( QPen { stroke_color, 2.0 } );
        painter.setBrush( brush_color );
        painter.drawPolygon( _selection_polygon );
    }
}
void EmbeddingViewer::wheelEvent( QWheelEvent* event )
{
    const auto extent = std::min( this->width(), this->height() ) - 20;
    const auto center = this->rect().center();

    const auto normalized = ( event->position() - center ) / ( extent / 2.0 );
    const auto x = ( normalized.x() - _projection_matrix( 0, 3 ) ) / _projection_matrix( 0, 0 );
    const auto y = ( -normalized.y() - _projection_matrix( 1, 3 ) ) / _projection_matrix( 1, 1 );

    constexpr auto sensitivity = 1.1;
    const auto old_scaling = _projection_matrix( 0, 0 );
    const auto new_scaling = old_scaling * ( event->angleDelta().y() >= 0 ? sensitivity : 1.0 / sensitivity );

    _projection_matrix( 0, 0 ) = new_scaling;                        // scale x
    _projection_matrix( 0, 3 ) = normalized.x() - new_scaling * x;   // translate x
    _projection_matrix( 1, 1 ) = new_scaling;                        // scale y
    _projection_matrix( 1, 3 ) = -normalized.y() - new_scaling * y;  // translate y
    _scatterplot_image_valid = false;
    this->update();
}

void EmbeddingViewer::mousePressEvent( QMouseEvent* event )
{
    if( event->button() == Qt::LeftButton )
    {
        _selection_polygon.append( event->position() );
        _selection_mode = SelectionMode::eGrowSegment;
    }
    else if( event->button() == Qt::RightButton )
    {
        _selection_polygon.append( event->position() );
        _selection_mode = SelectionMode::eShrinkSegment;
    }
}
void EmbeddingViewer::mouseReleaseEvent( QMouseEvent* event )
{
    if( event->button() == Qt::LeftButton || event->button() == Qt::RightButton )
    {
        if( _selection_polygon.size() == 1 )
        {
            if( event->button() == Qt::RightButton )
            {
                auto context_menu = QMenu {};

                _database.populate_segmentation_menu( context_menu );
                context_menu.addSeparator();

                auto point_size_menu = context_menu.addMenu( "Point Size" );
                auto point_size_action_group = new QActionGroup { point_size_menu };
                point_size_action_group->setExclusive( true );

                const auto size_options = std::vector<std::pair<const char*, GLfloat>> {
                    { "Very Large", 4.0f },
                    { "Large", 2.0f },
                    { "Medium", 1.0f },
                    { "Small", 0.5f },
                    { "Very Small", 0.2f },
                    { "Extremely Small", 0.1f },
                    { "Microscopic", 0.05f },
                    { "Atomic", 0.02f },
                    { "Subatomic", 0.01f },
                };

                for( const auto [label, size] : size_options )
                {
                    const auto action = point_size_menu->addAction( label, [this, size]
                    {
                        _point_size = size;
                        _scatterplot_image_valid = false;
                        this->update();
                    } );

                    action->setCheckable( true );
                    action->setChecked( _point_size == size );
                    point_size_action_group->addAction( action );
                }

                context_menu.addAction( "Reset View", [this] { this->reset_projection_matrix(); } );

                context_menu.addSeparator();
                context_menu.addAction( "Create Embedding", [this]
                {
                    this->import_embedding( EmbeddingCreator::execute( _database ) );
                } );
                context_menu.addAction( "Import Embedding", [this]
                {
                    auto selected_filter = QString { "*.mia" };
                    const auto filepath = QFileDialog::getOpenFileName( this, "Import Embedding...", "", "*.csv;;*.mia", &selected_filter, QFileDialog::DontUseNativeDialog );
                    this->import_embedding( std::filesystem::path { filepath.toStdWString() } );
                } );

                auto screenshot_menu = context_menu.addMenu( "Screenshot" );
                screenshot_menu->addAction( "1x Resolution", [this] { this->create_screenshot( 1 ); } );
                screenshot_menu->addAction( "2x Resolution", [this] { this->create_screenshot( 2 ); } );
                screenshot_menu->addAction( "4x Resolution", [this] { this->create_screenshot( 4 ); } );
                screenshot_menu->addAction( "Clipboard", [this] { this->create_screenshot( 0 ); } );

                context_menu.exec( event->globalPosition().toPoint() );
            }
        }
        else if( _selection_mode == SelectionMode::eGrowSegment || _selection_mode == SelectionMode::eShrinkSegment )
        {
            auto selection_polygon = Array<vec2<float>>::allocate( _selection_polygon.size() );
            for( qsizetype i = 0; i < _selection_polygon.size(); ++i )
            {
                const auto world = this->screen_to_world( _selection_polygon[i] );
                selection_polygon[i] = vec2<float> { static_cast<float>( world.x() ), static_cast<float>( world.y() ) };
            }

            const auto segment_number = _selection_mode == SelectionMode::eGrowSegment ? _database.active_segment()->number() : 0;
            const auto segmentation_numbers = _renderer->update_segmentation( _projection_matrix, selection_polygon, segment_number );

            auto editor = _database.segmentation()->editor();
            for( uint32_t element_index = 0; element_index < segmentation_numbers.size(); ++element_index )
            {
                editor.update_value( element_index, segmentation_numbers[element_index] );
            }
        }

        _selection_polygon.clear();
        _selection_mode = SelectionMode::eNone;
    }

    this->update();
}

void EmbeddingViewer::mouseMoveEvent( QMouseEvent* event )
{
    if( event->buttons() & ( Qt::LeftButton | Qt::RightButton ) )
    {
        if( _selection_mode == SelectionMode::eGrowSegment || _selection_mode == SelectionMode::eShrinkSegment )
        {
            _selection_polygon.append( event->position() );
            this->update();
        }
    }

    if( _search_tree )
    {
        const auto world = this->screen_to_world( event->position() );
        const auto nearest_neighbor_index = _search_tree->nearest_neighbor( vec2<float> { static_cast<float>( world.x() ), static_cast<float>( world.y() ) } );
        const auto nearest_neighbor_world = _point_positions[nearest_neighbor_index];
        const auto nearest_neighbor_screen = this->world_to_screen( QPointF { nearest_neighbor_world.x, nearest_neighbor_world.y } );

        if( QLineF { event->position(), nearest_neighbor_screen }.length() <= std::sqrt( _projection_matrix( 0, 0 ) ) * _point_size + 5.0 )
        {
            _database.update_highlighted_element_index( _point_indices[nearest_neighbor_index] );
        }
        else
        {
            _database.update_highlighted_element_index( std::nullopt );
        }
    }

    _cursor_position = event->position();
    this->update();
}
void EmbeddingViewer::leaveEvent( QEvent* event )
{
    _database.update_highlighted_element_index( std::nullopt );
    _cursor_position = QPointF { -1.0, -1.0 };
}

double EmbeddingViewer::world_to_screen_x( double value ) const
{
    const auto extent = std::min( this->width(), this->height() ) - 20;
    const auto center = this->rect().center();
    return center.x() + ( extent / 2.0 ) * ( value * _projection_matrix( 0, 0 ) + _projection_matrix( 0, 3 ) );
}
double EmbeddingViewer::world_to_screen_y( double value ) const
{
    const auto extent = std::min( this->width(), this->height() ) - 20;
    const auto center = this->rect().center();
    return center.y() + ( extent / 2.0 ) * -( value * _projection_matrix( 1, 1 ) + _projection_matrix( 1, 3 ) );
}
QPointF EmbeddingViewer::world_to_screen( const QPointF& world ) const
{
    return QPointF { this->world_to_screen_x( world.x() ), this->world_to_screen_y( world.y() ) };
}

double EmbeddingViewer::screen_to_world_x( double value ) const
{
    const auto extent = std::min( this->width(), this->height() ) - 20;
    const auto center = this->rect().center();
    return ( ( value - center.x() ) / ( extent / 2.0 ) - _projection_matrix( 0, 3 ) ) / _projection_matrix( 0, 0 );
}
double EmbeddingViewer::screen_to_world_y( double value ) const
{
    const auto extent = std::min( this->width(), this->height() ) - 20;
    const auto center = this->rect().center();
    return ( -( value - center.y() ) / ( extent / 2.0 ) - _projection_matrix( 1, 3 ) ) / _projection_matrix( 1, 1 );
}
QPointF EmbeddingViewer::screen_to_world( const QPointF& screen ) const
{
    return QPointF { this->screen_to_world_x( screen.x() ), this->screen_to_world_y( screen.y() ) };
}

void EmbeddingViewer::segmentation_changed()
{
    const auto segmentation = _database.segmentation();
    auto segment_colors = Array<vec4<float>>::allocate( segmentation->segment_count() );
    segment_colors[0] = vec4<float> { 0.9f, 0.9f, 0.9f, 1.0f };
    for( uint32_t segment_number = 1; segment_number < segmentation->segment_count(); ++segment_number )
    {
        segment_colors[segment_number] = segmentation->segment( segment_number )->color();
    }

    _renderer->update_segmentation( _database.segmentation()->segment_numbers(), segment_colors );
    _scatterplot_image_valid = false;
    this->update();
}
void EmbeddingViewer::reset_projection_matrix()
{
    _projection_matrix = QMatrix4x4 {};
    _projection_matrix( 0, 0 ) = 0.95f;
    _projection_matrix( 1, 1 ) = 0.95f;
    _scatterplot_image_valid = false;
    this->update();
}
void EmbeddingViewer::import_embedding( const std::filesystem::path& filepath )
{
    if( filepath.empty() )
    {
        return;
    }

    const auto extension = filepath.extension();
    if( extension == ".csv" )
    {
        auto stream = std::ifstream { filepath };
        if( !stream )
        {
            QMessageBox::critical( this, "Import Embedding...", "Failed to open file." );
            return;
        }

        while( stream )
        {
            std::string line;
            std::getline( stream, line );
            if( line.empty() ) continue;

            auto stringstream = std::stringstream { line };

            uint32_t index;
            ( stringstream >> index ).ignore( 1 );

            float x, y;
            ( stringstream >> x ).ignore( 1 );
            ( stringstream >> y ).ignore( 1 );

            _point_indices.push_back( index );
            _point_positions.push_back( vec2<float> { x, y } );
        }
    }
    else if( extension == ".mia" )
    {
        auto stream = MIAFileStream {};
        if( !stream.open( filepath, std::ios::in ) )
        {
            QMessageBox::critical( this, "Import Embedding...", "Failed to open file." );
            return;
        }

        const auto identifier = stream.read<std::string>();
        if( identifier != "Embedding" )
        {
            QMessageBox::critical( this, "Import Embedding...", "Invalid embedding file." );
            return;
        }

        const auto element_count = stream.read<uint32_t>();
        _point_indices.resize( element_count );
        _point_positions.resize( element_count );

        stream.read( _point_indices.data(), element_count * sizeof( uint32_t ) );
        stream.read( _point_positions.data(), element_count * sizeof( vec2<float> ) );
    }
    else
    {
        QMessageBox::critical( this, "Import Embedding...", "Unsupported file format: " + QString::fromStdString( extension.string() ) );
    }

    _search_tree.reset( new SearchTree { _point_positions } );
    _renderer->update_points( _point_positions, _point_indices );
    _scatterplot_image_valid = false;
    this->update();
}
void EmbeddingViewer::create_screenshot( uint32_t scaling ) const
{
    const auto filepath = scaling == 0 ? QString { "clipboard" } : QFileDialog::getSaveFileName( nullptr, "Export Image...", "", "*.png", nullptr, QFileDialog::DontUseNativeDialog );

    if( !filepath.isEmpty() )
    {
        scaling = std::max( scaling, 1u );

        _renderer->update_texture_size( scaling * EmbeddingRenderer::default_texture_size );
        const auto extent = std::min( this->width(), this->height() ) - 20;
        const auto adjusted_point_size = ( _point_size * std::sqrt( _projection_matrix( 0, 0 ) ) ) / 72.0 * QGuiApplication::primaryScreen()->physicalDotsPerInch() / ( static_cast<double>( extent ) / _renderer->texture_size() );
        const auto image = _renderer->render( _projection_matrix, adjusted_point_size );
        _renderer->update_texture_size( EmbeddingRenderer::default_texture_size );

        if( filepath == "clipboard" )
        {
            // Workaround for copying and image with transparency into the clipboard
            // https://stackoverflow.com/questions/1260253/how-do-i-put-an-qimage-with-transparency-onto-the-clipboard-for-another-applicat
            auto mime_data = new QMimeData();
            auto image_data = QByteArray {};
            auto image_buffer = QBuffer { &image_data };

            image_buffer.open( QIODevice::WriteOnly );
            image.save( &image_buffer, "PNG" );
            image_buffer.close();
            mime_data->setData( "PNG", image_data );

            QApplication::clipboard()->setMimeData( mime_data );
        }
        else
        {
            image.save( filepath );
        }
    }
}

// ----- EmbeddingViewer::SearchTree ----- //

EmbeddingViewer::SearchTree::SearchTree( const std::vector<vec2<float>>& points ) : _points { points }, _indices( _points.size() )
{
    std::iota( _indices.begin(), _indices.end(), 0 );
    this->construct_partitions_recursive( Partition::Axis::eX, 0, static_cast<uint32_t>( _points.size() ) );
}

const std::vector<vec2<float>>& EmbeddingViewer::SearchTree::points() const noexcept
{
    return _points;
}
uint32_t EmbeddingViewer::SearchTree::nearest_neighbor( vec2<float> point )
{
    auto current = uint32_t { 0 };
    while( true )
    {
        const auto& partition = _partitions[current];

        if( partition.axis == Partition::Axis::eNone )
        {
            struct
            {
                uint32_t index {};
                double distance {};
            } nearest { std::numeric_limits<uint32_t>::max(), std::numeric_limits<double>::max() };

            for( uint32_t i { partition.begin }; i < partition.end; ++i )
            {
                const double distance { ( _points[_indices[i]] - point ).length() };
                if( distance < nearest.distance ) nearest = { _indices[i], distance };
            }

            return nearest.index;
        }

        if( partition.axis == Partition::Axis::eX ) current = point.x < partition.value ? partition.lower : partition.upper;
        else current = point.y < partition.value ? partition.lower : partition.upper;
    }
}

uint32_t EmbeddingViewer::SearchTree::construct_partitions_recursive( Partition::Axis axis, uint32_t begin, uint32_t end )
{
    if( end - begin <= 16 )
    {
        const auto index = static_cast<uint32_t>( _partitions.size() );
        const auto partition = Partition {
            begin, end, Partition::Axis::eNone, 0.0, std::numeric_limits<uint32_t>::max(), std::numeric_limits<uint32_t>::max()
        };
        _partitions.push_back( partition );
        return index;
    }

    const auto comparison {
        axis == Partition::Axis::eX ?
        std::function<bool( uint32_t, uint32_t )> { [this]( uint32_t a, uint32_t b ) { return _points[a].x < _points[b].x; } } :
        std::function<bool( uint32_t, uint32_t )> { [this]( uint32_t a, uint32_t b ) { return _points[a].y < _points[b].y; } }
    };
    auto middle = ( begin + end ) / 2;
    std::nth_element( _indices.begin() + begin, _indices.begin() + middle, _indices.begin() + end, comparison );

    const auto point_value = [this, axis] ( uint32_t pointIndex )
    {
        return axis == Partition::Axis::eX ? _points[pointIndex].x : _points[pointIndex].y;
    };
    const auto value = point_value( _indices[middle] );
    for( ; middle < end - 1 && point_value( _indices[middle++ + 1] ) == value; );

    const auto partition = Partition {
        begin, end, axis, value, std::numeric_limits<uint32_t>::max(), std::numeric_limits<uint32_t>::max()
    };
    const auto index = static_cast<uint32_t>( _partitions.size() );
    _partitions.push_back( partition );

    const auto next_axis = Partition::Axis { axis == Partition::Axis::eX ? Partition::Axis::eY : Partition::Axis::eX };
    _partitions[index].lower = this->construct_partitions_recursive( next_axis, begin, middle );
    _partitions[index].upper = this->construct_partitions_recursive( next_axis, middle, end );

    return index;
}