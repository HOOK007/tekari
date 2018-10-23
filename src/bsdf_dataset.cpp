#define POWITACQ_IMPLEMENTATION
#include <tekari/bsdf_dataset.h>
#include <tekari/raw_data_processing.h>

TEKARI_NAMESPACE_BEGIN

inline powitacq::Vector3f enoki_to_powitacq_vec3(const Vector3f& v) { return powitacq::Vector3f(v[0], v[1], v[2]); }
inline Vector3f powitacq_to_enoki_vec3(const powitacq::Vector3f& v) { return Vector3f(v[0], v[1], v[2]); }

BSDFDataset::BSDFDataset(const string& file_path)
: m_brdf(file_path)
, m_n_theta(32)
, m_n_phi(32)
{
    // copy the wavelengths
    m_wavelengths.resize(m_brdf.wavelengths().size());
    std::copy(begin(m_brdf.wavelengths()), end(m_brdf.wavelengths()), begin(m_wavelengths));

    compute_wavelengths_colors();

    // artificially assign metadata members
    m_metadata.add_line(m_brdf.description());
    m_metadata.set_sample_name(file_path.substr(file_path.find_last_of("/") + 1, file_path.find_last_of(".")));
}

bool BSDFDataset::init()
{
    if (!Dataset::init())
        return false;
    set_incident_angle({0.0f, 0.0f});
    return true;
}

void BSDFDataset::set_intensity_index(size_t intensity_index)
{
    m_intensity_index = std::min(intensity_index, m_raw_measurement.n_wavelengths() + 1);;
    if (!m_cache_mask[m_intensity_index])
    {
        m_cache_mask[m_intensity_index] = true;

        m_brdf.sample_state(m_intensity_index-1, m_raw_measurement[m_intensity_index+2].data());
        compute_min_max_intensities(m_points_stats, m_raw_measurement, m_intensity_index);
        compute_normalized_heights(m_raw_measurement, m_points_stats, m_h, m_intensity_index);

        update_points_stats(m_points_stats, m_raw_measurement, m_v2d, m_h, m_intensity_index);
        update_selection_stats( m_selection_stats, m_selected_points, m_raw_measurement, m_v2d, m_h, m_intensity_index);

        compute_normals(m_f, m_v2d, m_h, m_n, m_intensity_index);
    }
    update_shaders_data();
}

void BSDFDataset::set_incident_angle(const Vector2f& incident_angle_)
{

    Vector2f incident_angle(incident_angle_);
    incident_angle.x() = std::max(incident_angle.x(), 1e-6f);
    cout << std::setw(50) << std::left << "Setting incident angle ..";
    Timer<> timer;

    vector<float> luminance;
    vector<powitacq::Vector3f> wos;
    vector<powitacq::Vector3f> colors;
    if (!m_brdf.set_state(enoki_to_powitacq_vec3(hemisphere_to_vec3<Vector3f>(incident_angle)), m_n_theta, m_n_phi, luminance, wos, colors))
    {
        cout << "done. (took " <<  time_string(timer.value()) << ")" << endl;
        return;
    }
    cout << "done. (took " <<  time_string(timer.value()) << ")" << endl;

    size_t n_intensities = m_brdf.wavelengths().size() + 1;     // account for luminance
    size_t n_sample_points = wos.size();

    m_raw_measurement.resize(n_intensities, n_sample_points);
    m_v2d.resize(n_sample_points);
    m_colors.resize(n_sample_points, 3);

    m_h[0].resize (n_intensities, n_sample_points);
    m_h[1].resize(n_intensities, n_sample_points);
    m_n[0].resize (n_intensities, n_sample_points);
    m_n[1].resize(n_intensities, n_sample_points);
    m_cache_mask.resize(n_intensities);
    m_points_stats.reset(n_intensities);
    m_selection_stats.reset(n_intensities);

    // clear mask
    m_cache_mask.assign(n_intensities, false);
    m_cache_mask[0] = true;                     // luminance is always computed

    // artificially assign metadata members
    m_metadata.set_incident_angle(incident_angle);
    m_metadata.set_points_in_file(m_raw_measurement.n_sample_points());
    m_selected_points.assign(m_raw_measurement.n_sample_points(), NOT_SELECTED_FLAG);

    for (size_t i = 0; i < wos.size(); ++i)
    {
        Vector2f outgoing_angle = vec3_to_hemisphere<Vector2f>(wos[i]);
        m_raw_measurement.set_theta(i, outgoing_angle.x());
        m_raw_measurement.set_phi(i, outgoing_angle.y());
        m_raw_measurement.set_luminance(i, luminance[i]);
        m_v2d[i] = vec3_to_disk<Vector2f>(wos[i]);

        m_colors[i][0] = colors[i][0];
        m_colors[i][1] = colors[i][1];
        m_colors[i][2] = colors[i][2];
    }

    triangulate_data(m_f, m_v2d);
    compute_path_segments(m_path_segments, m_v2d);

    // compute data for luminance
    compute_min_max_intensities(m_points_stats, m_raw_measurement, 0);
    compute_normalized_heights(m_raw_measurement, m_points_stats, m_h, 0);
    update_points_stats(m_points_stats, m_raw_measurement, m_v2d, m_h, 0);
    update_selection_stats(m_selection_stats, m_selected_points, m_raw_measurement, m_v2d, m_h, 0);
    compute_normals(m_f, m_v2d, m_h, m_n, 0);

    link_data_to_shaders();
    set_intensity_index(m_intensity_index);
}

void BSDFDataset::get_selection_spectrum(vector<float> &spectrum)
{
    size_t point_index = m_selection_stats[m_intensity_index].highest_point_index;
    powitacq::Spectrum s = m_brdf.sample_state(point_index);

    spectrum.clear();
    spectrum.reserve(s.size());

    float max = -std::numeric_limits<float>::max();
    for(size_t i = 0; i < s.size(); ++i)
    {
        if (m_wavelengths[i] > 360.0f && m_wavelengths[i] < 1000.0f)
        {
            spectrum.push_back(s[i]);
            max = std::max(max, s[i]);
        }
    }
    float normalization = 0.9f / max;
    // normalize spectrum
    for(size_t i = 0; i < spectrum.size(); ++i)
        spectrum[i] *= normalization;
}

TEKARI_NAMESPACE_END
